// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

#include "YnnkMetaFaceController.h"
#include "YnnkVoiceLipsyncData.h"
#include "YnnkLipsyncController.h"
#include "Components/SkeletalMeshComponent.h"
#include "MetaFaceTypes.h"
#include "YnnkMetaFaceEnhancer.h"
#include "MetaFaceFunctionLibrary.h"
#include "Interfaces/IPluginManager.h"
#include "Animation/PoseAsset.h"
#include "Sound/SoundWave.h"
#include "YnnkRemoteClient.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Kismet/KismetMathLibrary.h"
#include "NeuralProcessWrapper.h"
#include "HAL/CriticalSection.h"
#include "Engine/World.h"
#include "Async/Async.h"
// remove:
#include "YnnkTypes.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY(LogMetaFace);

#define __set_curve_value(CurveSet, Curve, Value) \
	if (float* Val = CurveSet.Find(Curve)) \
		*Val = Value; \
	else \
		CurveSet.Add(Curve, Value);

#define __is_anim_converted(animation) (animation.AnimationFlag && 1)
#define __set_anim_converted(animation) animation.AnimationFlag = 1

FCriticalSection UYnnkMetaFaceController::AsyncBuildMutex;

namespace JsonHelpers
{
	void LoadFromJsonToArray(const FString& ElelemtName, const TSharedPtr<FJsonObject>& JsonRoot, RawAnimDataMap& OutData)
	{
		OutData.Empty();

		if (!JsonRoot->Values.Contains(ElelemtName))
		{
			return;
		}

		const auto& Root = JsonRoot->Values[ElelemtName]->AsObject();

		for (const auto& Curve : Root->Values)
		{
			TArray<float> Values;

			const auto& FloatArr = Curve.Value->AsArray();
			Values.SetNumUninitialized(FloatArr.Num());
			int32 i = INDEX_NONE;
			for (const auto& val : FloatArr)
			{
				Values[++i] = (float)val->AsNumber();
			}

			OutData.Add(*Curve.Key, Values);
		}
	}
};

UYnnkMetaFaceController::UYnnkMetaFaceController()
	: HeadComponentName(TEXT("Face"))
	, BodyComponentName(TEXT("Body"))
	, EyeRightSetup(TEXT("FACIAL_R_Eye"), TEXT("CTRL_Expressions_eyeLookDownR"), TEXT("CTRL_Expressions_eyeLookLeftR"), TEXT("CTRL_Expressions_eyeLookRightR"), TEXT("CTRL_Expressions_eyeLookUpR"))
	, EyeLeftSetup(TEXT("FACIAL_L_Eye"), TEXT("CTRL_Expressions_eyeLookDownL"), TEXT("CTRL_Expressions_eyeLookLeftL"), TEXT("CTRL_Expressions_eyeLookRightL"), TEXT("CTRL_Expressions_eyeLookUpL"))
	, ArKitCurvesPoseAsset(nullptr)
	, EmotionsIntensity(1.f)
	, VisemeApplyAlpha(0.f)
	, LipsyncNeuralIntensity(1.f)
	, LipsyncSmoothness(0.3f)
	, FacialAnimationSmoothness(1.f)
	, bAsyncAnimationBuilder(true)
	, bApplyLipsyncToSpeak(true)
	, bApplyFacialAnimationToSpeak(true)
	, bLipSyncToSkeletonCurves(false)
	, bFacialAnimationToSkeletonCurves(false)
	, bUseExtraAnimationFromLipsyncDataAsset(true)
	, EyeMovementSpeed(280.f)
	, PlayTime(0.f)
	, bUseRemoteBuilder(false)
	, EyesControllerType(EEyesControlType::EC_Disabled)
	, EyeRotation_Right(FVector2D::ZeroVector)
	, EyeRotation_Left(FVector2D::ZeroVector)
	// protected
	, LipsyncController(nullptr)
	, HeadMesh(nullptr)
	, BodyMesh(nullptr)
	, ProcessedLipsyncData(nullptr)
	, RemoteClient(nullptr)
	, bDelayedSpeak(false)
	, DelayedSpeak_SoundWave(nullptr)
	, DelayedSpeak_TimeOffset(0.f)
	, EyesTargetLocation(FVector::ZeroVector)
	, EyesTargetComponent(nullptr)
	, EyesNextUpdateTime(0.f)
	, EyeRotation_Target(FVector2D::ZeroVector)
	, FacialAnimationPauseDuration(1.f)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

#if WITH_EDITOR
	if (!IsValid(ArKitCurvesPoseAsset))
	{
		UPoseAsset* Asset = FindObject<UPoseAsset>(FTopLevelAssetPath(TEXT("PoseAsset'/Game/MetaHumans/Common/Common/Mocap/mh_arkit_mapping_pose.mh_arkit_mapping_pose'")));
		if (Asset)
		{
			ArKitCurvesPoseAsset = Asset;
		}
		else
		{
			UE_LOG(LogMetaFace, Log, TEXT("mh_arkit_mapping_pose Pose Asset not found in current project"));
		}
	}
#endif
}

void UYnnkMetaFaceController::BeginPlay()
{
	Super::BeginPlay();

	// Reference to UYnnkLipsyncController
	auto Comp = GetOwner()->GetComponentByClass(UYnnkLipsyncController::StaticClass());
	if (Comp)
	{
		LipsyncController = Cast<UYnnkLipsyncController>(Comp);
		LipsyncController->OnStartSpeaking.AddDynamic(this, &UYnnkMetaFaceController::OnLipsyncController_StartSpeaking);
		LipsyncController->OnSpeakingInterrupted.AddDynamic(this, &UYnnkMetaFaceController::OnLipsyncController_SpeakingInterrupted);
	}

	auto comps = GetOwner()->GetComponents();
	for (auto& cmp : comps)
	{
		if (cmp->IsA(USkeletalMeshComponent::StaticClass()))
		{
			if (cmp->GetFName() == HeadComponentName)
			{
				HeadMesh = Cast<USkeletalMeshComponent>(cmp);
			}
			if (cmp->GetFName() == BodyComponentName)
			{
				BodyMesh = Cast<USkeletalMeshComponent>(cmp);
			}

			if (HeadMesh && BodyMesh)
			{
				break;
			}
		}
	}

	if (BodyComponentName.IsNone() && IsValid(HeadMesh))
	{
		BodyMesh = HeadMesh;
	}
}

void UYnnkMetaFaceController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (RemoteClient)
	{
		if (RemoteClient->IsConnected())
		{
			RemoteClient->Disconnect();
		}
	}

	if (IsValid(GetNeuralProcessor()))
	{
		GetNeuralProcessor()->InterruptAll();
	}

	if (IsValid(LipsyncController))
	{
		LipsyncController->OnStartSpeaking.RemoveDynamic(this, &UYnnkMetaFaceController::OnLipsyncController_StartSpeaking);
		LipsyncController->OnSpeakingInterrupted.RemoveDynamic(this, &UYnnkMetaFaceController::OnLipsyncController_SpeakingInterrupted);
		LipsyncController = nullptr;
	}
}

void UYnnkMetaFaceController::BeginDestroy()
{
	Super::BeginDestroy();

	if (LipsyncController)
	{
		LipsyncController->OnStartSpeaking.RemoveDynamic(this, &UYnnkMetaFaceController::OnLipsyncController_StartSpeaking);
		LipsyncController->OnSpeakingInterrupted.RemoveDynamic(this, &UYnnkMetaFaceController::OnLipsyncController_SpeakingInterrupted);
		LipsyncController = nullptr;
	}
	HeadMesh = nullptr;
	BodyMesh = nullptr;
	ProcessedLipsyncData = nullptr;
}

void UYnnkMetaFaceController::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Facial Animation

	bool bPlayFacialAnim = IsValid(LipsyncController) && (CurrentLipsync.IsActive() || CurrentFaceAnim.IsActive());
	if (bPlayFacialAnim)
	{
		PlayTime = LipsyncController->IsSpeaking()
			? LipsyncController->PlayTime
			: (PlayTime + DeltaTime);

		if (CurrentLipsync.IsActive())
		{
			CurrentLipsync.ProcessFrame(PlayTime, LipsyncController);
		}
		if (CurrentFaceAnim.IsActive())
		{
			CurrentFaceAnim.ProcessFrame(PlayTime, LipsyncController);
		}
		
		// Combine animation with YnnkVoiceController with default parameters
		if (bAutoBakeAnimation)
		{
			for (auto& Curve : CurrentBakedFaceFrame)
			{
				Curve.Value = 0.f;

				float SummAlpha = 0.f;
				const FName& CurveName = Curve.Key;

				if (LipsyncController->AnimationType == EYnnkAnimationType::AT_AnimationCurves)
				{
					if (float* Value = LipsyncController->ActiveCurveValues.Find(CurveName))
					{
						Curve.Value = *Value * BakedYnnkToMetaFaceRatio * BakedLipSyncIntensity;
						SummAlpha = BakedYnnkToMetaFaceRatio;
					}
				}
				
				if (CurrentLipsync.IsActive())
				{
					if (float* Value = CurrentLipsync.AnimationFrame.Find(CurveName))
					{
						Curve.Value += *Value * (1.f - SummAlpha) * BakedLipSyncIntensity;
					}
				}

				if (CurrentFaceAnim.IsActive())
				{
					if (float* Value = CurrentFaceAnim.AnimationFrame.Find(CurveName))
					{
						Curve.Value += *Value * BakedFacialAnimationIntensity;
					}
				}

				if (BakedLipSyncIntensity != 1.f && LipSyncARCurvesSet.Contains(CurveName))
				{
					Curve.Value *= BakedLipSyncIntensity;
				}

				Curve.Value = FMath::Clamp(Curve.Value, -1.f, 1.f);
			}
		}
	}
	else
	{
		PlayTime = 0.f;
	}

	// Eyes Animation

	if (HeadMesh && BodyMesh)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();

		if (EyesControllerType == EEyesControlType::EC_LiveMovement)
		{
			if (CurrentTime > EyesNextUpdateTime)
			{
				EyesNextUpdateTime = CurrentTime + FMath::RandRange(0.4f, 3.5f);
				EyeRotation_Target.X = FMath::FRandRange(-50.f, 50.f);
				EyeRotation_Target.Y = FMath::FRandRange(-30.f, 30.f);
			}

			EyeRotation_Right = FMath::Vector2DInterpConstantTo(EyeRotation_Right, EyeRotation_Target, DeltaTime, EyeMovementSpeed);
			EyeRotation_Left = FMath::Vector2DInterpConstantTo(EyeRotation_Left, EyeRotation_Target, DeltaTime, EyeMovementSpeed);
		}
		else if (EyesControllerType == EEyesControlType::EC_FocusAtTarget)
		{
			if (EyesTargetAlpha < 1.f)
			{
				EyesTargetAlpha += DeltaTime * 4.f;
				if (EyesTargetAlpha > 1.f) EyesTargetAlpha = 1.f;
			}

			FVector Target;
			if (EyesTargetComponent)
			{
				Target = EyesTargetComponent->GetComponentLocation();
				if (!EyesTargetLocation.IsZero())
				{
					Target += EyesTargetComponent->GetComponentRotation().RotateVector(EyesTargetLocation);
				}
			}
			else
			{
				Target = EyesTargetLocation;
			}

			FVector EyeSrcR = HeadMesh->GetSocketLocation(EyeRightSetup.BoneName);
			FVector EyeSrcL = HeadMesh->GetSocketLocation(EyeLeftSetup.BoneName);
			const FQuat HeadQ = BodyMesh->GetSocketRotation(TEXT("head")).Quaternion();

			const FRotator HeadRot = UKismetMathLibrary::MakeRotFromXZ(HeadQ.GetRightVector(), HeadQ.GetForwardVector());

			FRotator LookAtR = UKismetMathLibrary::MakeRotFromXZ(Target - EyeSrcR, HeadRot.Quaternion().GetUpVector());
			FRotator LookAtL = UKismetMathLibrary::MakeRotFromXZ(Target - EyeSrcL, HeadRot.Quaternion().GetUpVector());

			FRotator DeltaR = UKismetMathLibrary::NormalizedDeltaRotator(LookAtR, HeadRot);
			FRotator DeltaL = UKismetMathLibrary::NormalizedDeltaRotator(LookAtL, HeadRot);

			if (bDrawDebug)
			{
				DrawDebugCoordinateSystem(GetWorld(), (EyeSrcR + EyeSrcL) * 0.5f, HeadRot, 25.f, false, 0.05f, 0, 0.5f);

				FRotator r;
				r = UKismetMathLibrary::ComposeRotators(DeltaR, HeadRot);
				DrawDebugLine(GetWorld(), EyeSrcR, EyeSrcR + r.Vector() * 40.f, FColor::Magenta, false, 0.05f, 0, 0.5f);
				r = UKismetMathLibrary::ComposeRotators(DeltaL, HeadRot);
				DrawDebugLine(GetWorld(), EyeSrcL, EyeSrcL + r.Vector() * 40.f, FColor::Magenta, false, 0.05f, 0, 0.5f);

				DrawDebugLine(GetWorld(), EyeSrcR, EyeSrcR + LookAtR.Vector() * 80.f, FColor::Yellow, false, 0.05f, 0, 0.2f);
				DrawDebugLine(GetWorld(), EyeSrcL, EyeSrcL + LookAtL.Vector() * 80.f, FColor::Yellow, false, 0.05f, 0, 0.2f);
			}

			if (FMath::Abs(DeltaR.Yaw) > 50.f || FMath::Abs(DeltaL.Yaw) > 50.f)
			{
				EyeRotation_Right = FMath::Vector2DInterpConstantTo(EyeRotation_Right, FVector2D::ZeroVector, DeltaTime, EyeMovementSpeed * 1.5f);
				EyeRotation_Left = FMath::Vector2DInterpConstantTo(EyeRotation_Left, FVector2D::ZeroVector, DeltaTime, EyeMovementSpeed * 1.5f);
				EyesTargetAlpha = 0.f;
			}
			else
			{
				if (EyesTargetAlpha < 1.f)
				{
					EyeRotation_Right = FMath::Vector2DInterpConstantTo(EyeRotation_Right, FVector2D(DeltaR.Yaw, DeltaR.Pitch), DeltaTime, EyeMovementSpeed * (EyesTargetAlpha * 50.f + 1.f));
					EyeRotation_Left = FMath::Vector2DInterpConstantTo(EyeRotation_Left, FVector2D(DeltaL.Yaw, DeltaL.Pitch), DeltaTime, EyeMovementSpeed * (EyesTargetAlpha * 50.f + 1.f));
				}
				else
				{
					EyeRotation_Right = FVector2D(DeltaR.Yaw, DeltaR.Pitch);
					EyeRotation_Left = FVector2D(DeltaL.Yaw, DeltaL.Pitch);
				}
			}

		}
		else if (EyesControllerType == EEyesControlType::EC_Disabled)
		{
			// reset to zero
			EyeRotation_Right = FMath::Vector2DInterpConstantTo(EyeRotation_Right, FVector2D::ZeroVector, DeltaTime, EyeMovementSpeed);
			EyeRotation_Left = FMath::Vector2DInterpConstantTo(EyeRotation_Left, FVector2D::ZeroVector, DeltaTime, EyeMovementSpeed);
			if (FMath::Abs(EyeRotation_Right.X) < 0.01f && FMath::Abs(EyeRotation_Right.Y) < 0.01f)
			{
				EyeRotation_Right.X = EyeRotation_Right.Y = 0.f;
			}
			if (FMath::Abs(EyeRotation_Left.X) < 0.01f && FMath::Abs(EyeRotation_Left.Y) < 0.01f)
			{
				EyeRotation_Left.X = EyeRotation_Left.Y = 0.f;
			}
			if (!bPlayFacialAnim
				&& EyeRotation_Right.X == 0.f
				&& EyeRotation_Right.Y == 0.f
				&& EyeRotation_Left.X == 0.f
				&& EyeRotation_Left.Y == 0.f)
			{
				SetComponentTickEnabled(false);
			}
		}

		FillEyeAnimationCurves();
	}
}

bool UYnnkMetaFaceController::BuildFacialAnimationData(UYnnkVoiceLipsyncData* LipsyncData, bool bCreateLipSync, bool bCreateFacialAnimation)
{
	if (!IsValid(LipsyncController))
	{
		auto Comp = GetOwner()->GetComponentByClass(UYnnkLipsyncController::StaticClass());
		if (Comp)
		{
			LipsyncController = Cast<UYnnkLipsyncController>(Comp);
			LipsyncController->OnStartSpeaking.AddDynamic(this, &UYnnkMetaFaceController::OnLipsyncController_StartSpeaking);
			LipsyncController->OnSpeakingInterrupted.AddDynamic(this, &UYnnkMetaFaceController::OnLipsyncController_SpeakingInterrupted);
		}
		else
		{
			UE_LOG(LogMetaFace, Warning, TEXT("Can't find UYnnkLipsyncController component in %s"), *GetOwner()->GetName());
			return false;
		}
	}
	auto ModuleMFE = FModuleManager::GetModulePtr<FYnnkMetaFaceEnhancerModule>(TEXT("YnnkMetaFaceEnhancer"));
	if (!LipsyncData)
	{
		UE_LOG(LogMetaFace, Warning, TEXT("BuildFacialAnimationData: invalid lip-sync data"));
		return false;
	}
	if (LipsyncData->PhonemesData.Num() == 0)
	{
		UE_LOG(LogMetaFace, Warning, TEXT("LipsyncData asset (%s) doesn't contain phonemes data. Enable it in Project Settings > [Plugins] Ynnk Lip-sync > Generate Phonemes Data"), *LipsyncData->GetName());
		return false;
	}

	if (bUseRemoteBuilder)
	{
		// build using remote server

		ProcessedLipsyncData = LipsyncData;
		RemoteClient->GenerateFacialAnimation(LipsyncData, bCreateLipSync, bCreateFacialAnimation);
	}
	else if (bAsyncAnimationBuilder)
	{
		if (bAsyncWorkerIsActive)
		{
			bExecutionInterrupted = true;
			GetNeuralProcessor()->InterruptAll();
		}

		ProcessedLipsyncData = LipsyncData;

		// May freeze game in theory, but single symbol is actually processed quickly enough
		// @TODO: lock within async
		AsyncBuildMutex.Lock();
		AsyncBuildAnimation(LipsyncData);
		return true;
	}
	else // build locally without async builder
	{
		ProcessedLipsyncData = LipsyncData;

		// Generate animation
		FMHFacialAnimation LipsyncAnimation, FacialAnimation;
		RawAnimDataMap RawData;
		if (bCreateLipSync)
		{
			if (ModuleMFE->ProcessPhonemesData(LipsyncData->PhonemesData, true, RawData))
			{
				if (RawData.Num() > 0)
				{
					TMap<FName, FSimpleFloatCurve> AnimationData;
					UMFFunctionLibrary::RawDataToLipsync(ProcessedLipsyncData, RawData, AnimationData, FMetaFaceGenerationSettings(this));

					const FName FrownL = TEXT("MouthFrownLeft");
					const FName FrownR = TEXT("MouthFrownRight");
					const FName SmileL = TEXT("MouthSmileLeft");
					const FName SmileR = TEXT("MouthSmileRight");

					if (bBalanceSmileFrownCurves
						&& AnimationData.Contains(FrownL) && AnimationData.Contains(SmileL)
						&& AnimationData.Contains(FrownR) && AnimationData.Contains(SmileR))
					{
						for (int32 i = 0; i < AnimationData[FrownL].Values.Num(); i++)
						{
							if (AnimationData[FrownL].Values[i].Value < AnimationData[SmileL].Values[i].Value)
							{
								float Mean = (AnimationData[FrownL].Values[i].Value + AnimationData[SmileL].Values[i].Value) * 0.5f;
								AnimationData[FrownL].Values[i].Value = AnimationData[SmileL].Values[i].Value = Mean;
							}

							if (AnimationData[FrownR].Values[i].Value < AnimationData[SmileR].Values[i].Value)
							{
								float Mean = (AnimationData[FrownR].Values[i].Value + AnimationData[SmileR].Values[i].Value) * 0.5f;
								AnimationData[FrownR].Values[i].Value = AnimationData[SmileR].Values[i].Value = Mean;
							}
						}
					}

					if (bLipSyncToSkeletonCurves)
					{
						UMFFunctionLibrary::ConvertFacialAnimCurves(AnimationData, ArKitCurvesPoseAsset);
					}
					LipsyncAnimation.Initialize(AnimationData, false);
				}
			}
		}
		if (bCreateFacialAnimation)
		{
			if (ModuleMFE->ProcessPhonemesData(LipsyncData->PhonemesData, false, RawData))
			{
				if (RawData.Num() > 0)
				{
					TMap<FName, FSimpleFloatCurve> AnimationData;
					UMFFunctionLibrary::RawDataToFacialAnimation(ProcessedLipsyncData, RawData, AnimationData, FMetaFaceGenerationSettings(this));
					if (bBalanceSmileFrownCurves)
					{
						AnimationData.Remove(TEXT("MouthSmileLeft"));
						AnimationData.Remove(TEXT("MouthSmileRight"));
					}
					if (bFacialAnimationToSkeletonCurves)
					{
						UMFFunctionLibrary::ConvertFacialAnimCurves(AnimationData, ArKitCurvesPoseAsset);
					}

					FacialAnimation.Initialize(AnimationData, true, FacialAnimationPauseDuration, FacialAnimationPauseDuration * 0.5f - 0.01f);
					FacialAnimation.Intensity = EmotionsIntensity;
				}
			}
		}

		// Create animations preset
		FFacialAnimCollection NewItem;
		NewItem.LipSync = LipsyncAnimation;
		NewItem.FacialAnimation = FacialAnimation;

		if (bLipSyncToSkeletonCurves && !__is_anim_converted(LipsyncAnimation))
		{
			auto AnimCopy = LipsyncAnimation.AnimationData;
			UMFFunctionLibrary::ConvertFacialAnimCurves(AnimCopy, ArKitCurvesPoseAsset);
			NewItem.LipSync.AnimationData.Append(AnimCopy);
			__set_anim_converted(LipsyncAnimation);
		}

		if (bFacialAnimationToSkeletonCurves && !__is_anim_converted(FacialAnimation))
		{
			auto AnimCopy = FacialAnimation.AnimationData;
			UMFFunctionLibrary::ConvertFacialAnimCurves(AnimCopy, ArKitCurvesPoseAsset);
			NewItem.FacialAnimation.AnimationData.Append(AnimCopy);
			__set_anim_converted(FacialAnimation);
		}

		FaceAnimations.Add(ProcessedLipsyncData->GetFName(), NewItem);

		if (bDelayedSpeak)
		{
			SpeakEx(ProcessedLipsyncData, DelayedSpeak_SoundWave, DelayedSpeak_TimeOffset);
		}
		else
		{
			OnAnimationBuildingComplete.Broadcast(ProcessedLipsyncData, true);
		}
	}

	return false;
}

void UYnnkMetaFaceController::SpeakEx(UYnnkVoiceLipsyncData* VoiceLipsyncData, USoundWave* Sound, float SoundOffset)
{
	if (!LipsyncController)
	{
		UE_LOG(LogMetaFace, Log, TEXT("SpeakEx: invalid VoiceLipsyncData"));
		return;
	}

	bool bContainsData
		= (VoiceLipsyncData->ExtraAnimData1.Num() > 0 || !bApplyLipsyncToSpeak)
		&& (VoiceLipsyncData->ExtraAnimData2.Num() > 0 || !bApplyFacialAnimationToSpeak);

	if ((bUseExtraAnimationFromLipsyncDataAsset && bContainsData) || FaceAnimations.Contains(VoiceLipsyncData->GetFName()))
	{
		LipsyncController->SpeakEx(Sound, VoiceLipsyncData, SoundOffset);
	}
	else
	{
		bDelayedSpeak = true;
		DelayedSpeak_SoundWave = Sound;
		DelayedSpeak_TimeOffset = SoundOffset;
		BuildFacialAnimationData(VoiceLipsyncData, bApplyLipsyncToSpeak, bApplyFacialAnimationToSpeak);
	}
}

void UYnnkMetaFaceController::Speak(UYnnkVoiceLipsyncData* VoiceLipsyncData)
{
	if (VoiceLipsyncData)
	{
		SpeakEx(VoiceLipsyncData, VoiceLipsyncData->SoundAsset, 0.f);
	}
	else
	{
		UE_LOG(LogMetaFace, Warning, TEXT("Trying to speak invalid VoiceLipsyncData"));
	}
}

bool UYnnkMetaFaceController::InitializeRemoteAnimationBuilder(UYnnkRemoteClient*& RemoteConnectionClient, FString IPv4, int32 Port)
{
	if (IsValid(RemoteClient))
	{
		RemoteConnectionClient = RemoteClient;
		return true;
	}

	RemoteClient = NewObject<UYnnkRemoteClient>(UYnnkRemoteClient::StaticClass());
	if (RemoteClient)
	{
		//RemoteClient->bDebugLog = true;
		bUseRemoteBuilder = RemoteClient->ConnectToServer(IPv4, Port);
		if (bUseRemoteBuilder)
		{
			RemoteClient->OnResponseReceived.AddDynamic(this, &UYnnkMetaFaceController::OnRemoteClient_ResponseReceived);
			RemoteConnectionClient = RemoteClient;
			return true;
		}
	}

	RemoteConnectionClient = nullptr;
	return false;
}

void UYnnkMetaFaceController::DisconnectFromRemoteBuilder()
{
	if (bUseRemoteBuilder)
	{
		if (RemoteClient)
		{
			RemoteClient->Disconnect();
			__uev_destory_object(RemoteClient);
			RemoteClient = nullptr;
		}
		bUseRemoteBuilder = false;
	}
}

bool UYnnkMetaFaceController::SetUseRemoteAnimationBuilder(bool bNewIsEnabled)
{
	if (bUseRemoteBuilder == bNewIsEnabled)
	{
		return true;
	}

	if (bNewIsEnabled)
	{
		if (RemoteClient && RemoteClient->IsConnected())
		{
			bUseRemoteBuilder = true;
			return true;
		}		
	}
	else
	{
		bUseRemoteBuilder = false;
		return true;
	}

	return false;
}

void UYnnkMetaFaceController::SetEyesMovementEnabled(bool bNewIsEnabled)
{
	EyesControllerType = bNewIsEnabled
		? EEyesControlType::EC_LiveMovement
		: EEyesControlType::EC_Disabled;

	if (bNewIsEnabled)
	{
		SetComponentTickEnabled(true);
	}
}

void UYnnkMetaFaceController::SetEyesTarget(FVector TargetLocation, USceneComponent* TargetComponent)
{
	EyesTargetLocation = TargetLocation;
	EyesTargetComponent = TargetComponent;
	EyesControllerType = EEyesControlType::EC_FocusAtTarget;
	EyesTargetAlpha = 0.f;
	SetComponentTickEnabled(true);
}

bool UYnnkMetaFaceController::IsInitialized() const
{
	return IsValid(LipsyncController);
}

void UYnnkMetaFaceController::HasValidAnimation(const UYnnkVoiceLipsyncData* LipsyncData, bool& bLipsyncIsValid, bool& bFacialAnimationIsValid) const
{
	if (const auto AnimData = FaceAnimations.Find(LipsyncData->GetFName()))
	{
		bLipsyncIsValid = AnimData->LipSync.IsValid();
		bFacialAnimationIsValid = AnimData->FacialAnimation.IsValid();
	}
	else
	{
		bLipsyncIsValid = bFacialAnimationIsValid = false;
	}
}

UYnnkRemoteClient* UYnnkMetaFaceController::GetRemoteConnectionClient() const
{
	return RemoteClient;
}

UNeuralProcessWrapper* UYnnkMetaFaceController::GetNeuralProcessor() const
{
	auto ModuleMFE = FModuleManager::GetModulePtr<FYnnkMetaFaceEnhancerModule>(TEXT("YnnkMetaFaceEnhancer"));
	return ModuleMFE->GetNeuralProcessor();
}

void UYnnkMetaFaceController::AsyncBuildAnimation(UYnnkVoiceLipsyncData* LsData)
{
	bAsyncWorkerIsActive = true;
	bExecutionInterrupted = false;

	if (bLogDebug)
	{
		UE_LOG(LogMetaFace, Log, TEXT("AsyncBuildAnimation(\"%s\")"), *LsData->Subtitles.ToString());
	}

	Async(EAsyncExecution::Thread, [this, LsData]()
	{
		FString OutData;
		bool bResult = false;
		float TimeOffset = 0.f;
		UNeuralProcessWrapper* NeuralProcessor = GetNeuralProcessor();

		RawAnimDataMap GeneratedData;
		// Note: Updated from EAsyncExecution::Thread
		TMap<FName, FSimpleFloatCurve> OutLipsyncData;
		// Note: Updated from EAsyncExecution::Thread
		TMap<FName, FSimpleFloatCurve> OutFacialAnimationData;

		// Lip-sync
		if (bApplyLipsyncToSpeak && !bExecutionInterrupted)
		{
			if (NeuralProcessor->ProcessPhonemesData(LsData->PhonemesData, true, GeneratedData))
			{
				if (!bExecutionInterrupted)
				{
					bool bSavedLipSyncToSkeletonCurves = bLipSyncToSkeletonCurves;
					bLipSyncToSkeletonCurves = false;
					UMFFunctionLibrary::RawDataToLipsync(LsData, GeneratedData, OutLipsyncData, FMetaFaceGenerationSettings(this));
					bLipSyncToSkeletonCurves = bSavedLipSyncToSkeletonCurves;

					const FName FrownL = TEXT("MouthFrownLeft");
					const FName FrownR = TEXT("MouthFrownRight");
					const FName SmileL = TEXT("MouthSmileLeft");
					const FName SmileR = TEXT("MouthSmileRight");

					if (bBalanceSmileFrownCurves
						&& OutLipsyncData.Contains(FrownL) && OutLipsyncData.Contains(SmileL)
						&& OutLipsyncData.Contains(FrownR) && OutLipsyncData.Contains(SmileR))
					{
						for (int32 i = 0; i < OutLipsyncData[FrownL].Values.Num(); i++)
						{
							if (OutLipsyncData[FrownL].Values[i].Value < OutLipsyncData[SmileL].Values[i].Value)
							{
								float Mean = (OutLipsyncData[FrownL].Values[i].Value + OutLipsyncData[SmileL].Values[i].Value) * 0.5f;
								OutLipsyncData[FrownL].Values[i].Value = OutLipsyncData[SmileL].Values[i].Value = Mean;
							}

							if (OutLipsyncData[FrownR].Values[i].Value < OutLipsyncData[SmileR].Values[i].Value)
							{
								float Mean = (OutLipsyncData[FrownR].Values[i].Value + OutLipsyncData[SmileR].Values[i].Value) * 0.5f;
								OutLipsyncData[FrownR].Values[i].Value = OutLipsyncData[SmileR].Values[i].Value = Mean;
							}
						}
					}
					/*
					if (bLipSyncToSkeletonCurves)
					{
						UMFFunctionLibrary::ConvertFacialAnimCurves(OutLipsyncData, ArKitCurvesPoseAsset);
					}
					*/
				}
				else
				{
					if (bLogDebug)
					{
						UE_LOG(LogMetaFace, Log, TEXT("AsyncBuildAnimation [Working Thread]: execution was interrupted (1)"), *LsData->Subtitles.ToString());
					}
				}
			}
			else
			{
				OutLipsyncData.Empty();
				OutData = TEXT("ERROR: can't process data");
			}
		}

		// Facial Animation
		if (bApplyFacialAnimationToSpeak && !bExecutionInterrupted)
		{
			GeneratedData.Empty();
			if (NeuralProcessor->ProcessPhonemesData2(LsData->PhonemesData, false, GeneratedData))
			{
				if (!bExecutionInterrupted)
				{
					bool bSavedFacialAnimationToSkeletonCurves = bFacialAnimationToSkeletonCurves;
					bFacialAnimationToSkeletonCurves = false;
					UMFFunctionLibrary::RawDataToFacialAnimation(LsData, GeneratedData, OutFacialAnimationData, FMetaFaceGenerationSettings(this));
					bFacialAnimationToSkeletonCurves = bSavedFacialAnimationToSkeletonCurves;
					if (bBalanceSmileFrownCurves)
					{
						OutFacialAnimationData.Remove(TEXT("MouthSmileLeft"));
						OutFacialAnimationData.Remove(TEXT("MouthSmileRight"));
					}
					/*
					if (bFacialAnimationToSkeletonCurves)
					{
						UMFFunctionLibrary::ConvertFacialAnimCurves(OutFacialAnimationData, ArKitCurvesPoseAsset);
					}
					*/
				}
				else
				{
					if (bLogDebug)
					{
						UE_LOG(LogMetaFace, Log, TEXT("AsyncBuildAnimation [Working Thread]: execution was interrupted (2)"), *LsData->Subtitles.ToString());
					}
				}
			}
			else
			{
				OutFacialAnimationData.Empty();
				OutData = TEXT("ERROR: can't process data");
			}
		}

		// send result
		// 
		// Main job done here
		if (!bExecutionInterrupted && ProcessedLipsyncData == LsData)
		{
			AsyncTask(ENamedThreads::GameThread, [this, OutLipsyncData, OutFacialAnimationData]()
			{
				OnAsyncBuilder_AnimationCreated(OutLipsyncData, OutFacialAnimationData);
			});
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this]()
			{
				OnAsyncBuilder_RequestInterrupted();
			});
		}
	});
}

void UYnnkMetaFaceController::OnAsyncBuilder_RequestInterrupted()
{
	AsyncBuildMutex.Unlock();
	bAsyncWorkerIsActive = false;
	bExecutionInterrupted = false;

	if (bLogDebug)
	{
		UE_LOG(LogMetaFace, Log, TEXT("OnAsyncBuilder_RequestInterrupted"));
	}
}

void UYnnkMetaFaceController::OnAsyncBuilder_AnimationCreated(const TMap<FName, FSimpleFloatCurve>& LipsyncData, const TMap<FName, FSimpleFloatCurve>& FacialAnimationData)
{
	AsyncBuildMutex.Unlock();
	bAsyncWorkerIsActive = false;
	if (bExecutionInterrupted)
	{
		bExecutionInterrupted = false;
		return;
	}

	if (bLogDebug)
	{
		UE_LOG(LogMetaFace, Log, TEXT("OnAsyncBuilder_AnimationCreated(%d, %d)"), LipsyncData.Num(), FacialAnimationData.Num());
	}

	// Initialize animation structs from raw curves sets
	FMHFacialAnimation LipsyncAnimation, FacialAnimation;

	if (bApplyLipsyncToSpeak)
	{
		LipsyncAnimation.Initialize(LipsyncData, false);
		if (!LipsyncAnimation.IsValid())
		{
			if (bLogDebug)
			{
				UE_LOG(LogMetaFace, Log, TEXT("Generated lip-sync animation is corrupted"));
			}
			return;
		}
	}
	if (bApplyFacialAnimationToSpeak)
	{
		FacialAnimation.Initialize(FacialAnimationData, true, 1.f, 0.49f);
		if (!FacialAnimation.IsValid())
		{
			if (bLogDebug)
			{
				UE_LOG(LogMetaFace, Log, TEXT("Generated facial animation is corrupted"));
			}
			return;
		}
	}

	// Save result
	if (IsValid(ProcessedLipsyncData))
	{
		FFacialAnimCollection NewItem;
		NewItem.LipSync = LipsyncAnimation;
		NewItem.FacialAnimation = FacialAnimation;

		if (bLipSyncToSkeletonCurves && !__is_anim_converted(LipsyncAnimation))
		{
			if (bLogDebug)
			{
				UE_LOG(LogMetaFace, Log, TEXT("Converting lip-sync (AR curves) to skeletal animation using pose asset"));
			}

			auto AnimCopy = LipsyncAnimation.AnimationData;
			UMFFunctionLibrary::ConvertFacialAnimCurves(AnimCopy, ArKitCurvesPoseAsset);
			NewItem.LipSync.AnimationData.Append(AnimCopy);
			__set_anim_converted(NewItem.LipSync);
		}

		if (bFacialAnimationToSkeletonCurves && !__is_anim_converted(FacialAnimation))
		{
			if (bLogDebug)
			{
				UE_LOG(LogMetaFace, Log, TEXT("Converting emotions animation (AR curves) to skeletal animation using pose asset"));
			}

			auto AnimCopy = FacialAnimation.AnimationData;
			UMFFunctionLibrary::ConvertFacialAnimCurves(AnimCopy, ArKitCurvesPoseAsset);
			NewItem.FacialAnimation.AnimationData.Append(AnimCopy);
			__set_anim_converted(NewItem.FacialAnimation);
		}

		if (bLogDebug)
		{
			UE_LOG(LogMetaFace, Log, TEXT("Facial animation created (%d, %d)"), LipsyncAnimation.AnimationData.Num(), FacialAnimation.AnimationData.Num());
		}

		FaceAnimations.Add(ProcessedLipsyncData->GetFName(), NewItem);

		if (bDelayedSpeak)
		{
			if (bLogDebug)
			{
				UE_LOG(LogMetaFace, Log, TEXT("Generated animation is saved in component cache. Continue speaking [%s] with MetaFace animation"), *ProcessedLipsyncData->Subtitles.ToString());
			}
			SpeakEx(ProcessedLipsyncData, DelayedSpeak_SoundWave, DelayedSpeak_TimeOffset);
		}
		else
		{
			OnAnimationBuildingComplete.Broadcast(ProcessedLipsyncData, true);
		}

		ProcessedLipsyncData = nullptr;
	}
	else
	{
		OnAnimationBuildingComplete.Broadcast(ProcessedLipsyncData, false);
		if (bLogDebug)
		{
			UE_LOG(LogMetaFace, Log, TEXT("Can't continue speaking, because has no correct lip-sync asset"));
		}
	}
	bDelayedSpeak = false;
}

void UYnnkMetaFaceController::OnRemoteClient_ResponseReceived(int32 RequestID, const FString& Command, const FString& JsonPacket)
{
	bool bLipSync = Command.Contains(TEXT("lipsync"));
	bool bFaceAnim = Command.Contains(TEXT("facial"));

	if (!bLipSync && !bFaceAnim)
	{
		UE_LOG(LogMetaFace, Log, TEXT("Invalid input command: %s"), *Command);
	}

	// not out case, not our message
	if (!bLipSync && !bFaceAnim) return;
	if (!ProcessedLipsyncData)
	{
		// something is wrong
		UE_LOG(LogMetaFace, Warning, TEXT("ProcessedLipsyncData is invalid"));
		return;
	}

	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonPacket);
	TSharedPtr<FJsonObject> JsonObject;

	if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogMetaFace, Warning, TEXT("Invalid json data: %s"), *JsonPacket.Left(512));
		// Invalid packet
		return;
	}

	// Generate animation
	FMHFacialAnimation LipsyncAnimation, FacialAnimation;
	RawAnimDataMap RawData;
	if (bLipSync)
	{
		JsonHelpers::LoadFromJsonToArray(TEXT("lipsync"), JsonObject, RawData);
		if (RawData.Num() > 0)
		{
			TMap<FName, FSimpleFloatCurve> AnimationData;
			UMFFunctionLibrary::RawDataToLipsync(ProcessedLipsyncData, RawData, AnimationData, FMetaFaceGenerationSettings(this));
			LipsyncAnimation.Initialize(AnimationData, false);
		}
	}
	if (bFaceAnim)
	{
		JsonHelpers::LoadFromJsonToArray(TEXT("facial"), JsonObject, RawData);
		if (RawData.Num() > 0)
		{
			TMap<FName, FSimpleFloatCurve> AnimationData;
			UMFFunctionLibrary::RawDataToFacialAnimation(ProcessedLipsyncData, RawData, AnimationData, FMetaFaceGenerationSettings(this));
			FacialAnimation.Initialize(AnimationData, true, FacialAnimationPauseDuration, FacialAnimationPauseDuration * 0.5f - 0.01f);
			FacialAnimation.Intensity = EmotionsIntensity;
		}
	}

	// Create animations preset
	FFacialAnimCollection NewItem;
	NewItem.LipSync = LipsyncAnimation;
	NewItem.FacialAnimation = FacialAnimation;
	FaceAnimations.Add(ProcessedLipsyncData->GetFName(), NewItem);

	if (bDelayedSpeak)
	{
		SpeakEx(ProcessedLipsyncData, DelayedSpeak_SoundWave, DelayedSpeak_TimeOffset);
	}
	else
	{
		OnAnimationBuildingComplete.Broadcast(ProcessedLipsyncData, true);
	}

	ProcessedLipsyncData = nullptr;
	bDelayedSpeak = false;
}

void UYnnkMetaFaceController::OnLipsyncController_StartSpeaking(UYnnkVoiceLipsyncData* PhraseAsset)
{
	auto CachedAnimations = FaceAnimations.Find(PhraseAsset->GetFName());
	if (CachedAnimations)
	{
		CurrentLipsync = CachedAnimations->LipSync;
		CurrentFaceAnim = CachedAnimations->FacialAnimation;
	}
	else if (bUseExtraAnimationFromLipsyncDataAsset)
	{
		auto LipsCopy = PhraseAsset->ExtraAnimData1;
		if (bLipSyncToSkeletonCurves)
		{
			// convert to target curves using pose asset
			UMFFunctionLibrary::ConvertFacialAnimCurves(LipsCopy, ArKitCurvesPoseAsset);
			// but keep original curves, for example, to fix bones animation
			LipsCopy.Append(PhraseAsset->ExtraAnimData1);
		}
		CurrentLipsync.Initialize(LipsCopy, false);

		auto AnimCopy = PhraseAsset->ExtraAnimData2;
		if (bFacialAnimationToSkeletonCurves)
		{
			// convert to target curves using pose asset
			UMFFunctionLibrary::ConvertFacialAnimCurves(AnimCopy, ArKitCurvesPoseAsset);
			// keep original curves
			AnimCopy.Append(PhraseAsset->ExtraAnimData2);
		}
		CurrentFaceAnim.Initialize(AnimCopy, true, 1.f, 0.49f);

		if (bLogDebug)
		{
			UE_LOG(LogMetaFace, Log, TEXT("DEBUG. Playing facial animation stored in UYnnkVoiceLipsyncData asset %s"), *PhraseAsset->GetName());
		}
	}
	else
	{
		return;
	}

	if (bAutoBakeAnimation)
	{
		if (LipSyncARCurvesSet.IsEmpty())
		{
			UMFFunctionLibrary::GetMetaFaceCurvesSet(LipSyncARCurvesSet, true);
		}
		
		CurrentBakedFaceFrame.Empty();

		for (const auto& Curve : PhraseAsset->RawCurves)
			CurrentBakedFaceFrame.Add(Curve.Key);

		if (bApplyLipsyncToSpeak && CurrentLipsync.IsValid())
		{
			for (const auto& Curve : CurrentLipsync.AnimationData)
				CurrentBakedFaceFrame.Add(Curve.Key);
		}
		if (bApplyFacialAnimationToSpeak && CurrentFaceAnim.IsValid())
		{
			for (const auto& Curve : CurrentFaceAnim.AnimationData)
				CurrentBakedFaceFrame.Add(Curve.Key);
		}
	}

	// Play animation?
	if (bApplyLipsyncToSpeak && CurrentLipsync.IsValid()) CurrentLipsync.Play();
	if (bApplyFacialAnimationToSpeak && CurrentFaceAnim.IsValid()) CurrentFaceAnim.Play();
	SetComponentTickEnabled(true);
}

bool UYnnkMetaFaceController::CheckAnimationCurvesSetIsArKit(const TMap<FName, FSimpleFloatCurve>& Animation, bool bLipSyncCurves) const
{
	// get arkit curves
	TArray<FName> CompareSet;
	UMFFunctionLibrary::GetMetaFaceCurvesSet(CompareSet, bLipSyncCurves);

	// has more curves already?
	if (Animation.Num() + 3 > CompareSet.Num())
	{
		return false;
	}

	// has more then N curves different then arkit?
	int32 AlienCurvesNum = 0;
	for (const auto& Curve : CompareSet)
	{
		if (!Animation.Contains(Curve))
		{
			if (++AlienCurvesNum > 8 /* N */)
			{
				return false;
			}
		}
	}

	return true;
}

void UYnnkMetaFaceController::OnLipsyncController_SpeakingInterrupted(UYnnkVoiceLipsyncData* PhraseAsset)
{
	if (CurrentLipsync.IsValid())
	{
		CurrentLipsync.Stop();
	}
	if (CurrentFaceAnim.IsValid())
	{
		CurrentFaceAnim.Stop();
	}
}

void UYnnkMetaFaceController::FillEyeAnimationCurves()
{
	/*
	const FName EyeLookLeftL = TEXT("CTRL_Expressions_eyeLookLeftL");
	const FName EyeLookLeftR = TEXT("CTRL_Expressions_eyeLookLeftR");
	const FName EyeLookRightL = TEXT("CTRL_Expressions_eyeLookRightL");
	const FName EyeLookRightR = TEXT("CTRL_Expressions_eyeLookRightR");
	const FName EyeLookDownL = TEXT("CTRL_Expressions_eyeLookDownL");
	const FName EyeLookDownR = TEXT("CTRL_Expressions_eyeLookDownR");
	const FName EyeLookUpL = TEXT("CTRL_Expressions_eyeLookUpL");
	const FName EyeLookUpR = TEXT("CTRL_Expressions_eyeLookUpR");
	*/
	// ArKit curves
	const FName EyeLookLeftL = TEXT("EyeLookOutLeft");
	const FName EyeLookLeftR = TEXT("EyeLookInRight");
	const FName EyeLookRightL = TEXT("EyeLookInLeft");
	const FName EyeLookRightR = TEXT("EyeLookOutRight");
	const FName EyeLookDownL = TEXT("EyeLookDownLeft");
	const FName EyeLookDownR = TEXT("EyeLookDownRight");
	const FName EyeLookUpL = TEXT("EyeLookUpLeft");
	const FName EyeLookUpR = TEXT("EyeLookUpRight");

	float HorValueR = FMath::Clamp(EyeRotation_Right.X, -AllowedEyeRotationRightLeft, AllowedEyeRotationRightLeft) / MaxEyeRotationRightLeft;
	float VertValueR = FMath::Clamp(EyeRotation_Right.Y, -AllowedEyeRotationUpDown, AllowedEyeRotationUpDown) / MaxEyeRotationUpDown;
	float HorValueL = FMath::Clamp(EyeRotation_Left.X, -AllowedEyeRotationRightLeft, AllowedEyeRotationRightLeft) / MaxEyeRotationRightLeft;
	float VertValueL = FMath::Clamp(EyeRotation_Left.Y, -AllowedEyeRotationUpDown, AllowedEyeRotationUpDown) / MaxEyeRotationUpDown;

	float LookLeftR = 0.f, LookRightR = 0.f, LookDownR = 0.f, LookUpR = 0.f;
	if (HorValueR > 0.f) LookRightR = HorValueR; else LookLeftR = -HorValueR;
	if (VertValueR > 0.f) LookUpR = VertValueR; else LookDownR = -VertValueR;

	float LookLeftL = 0.f, LookRightL = 0.f, LookDownL = 0.f, LookUpL = 0.f;
	if (HorValueL > 0.f) LookRightL = HorValueL; else LookLeftL = -HorValueL;
	if (VertValueL > 0.f) LookUpL = VertValueL; else LookDownL = -VertValueL;
	
	// LookLeft
	__set_curve_value(CurrentEyesState, EyeLeftSetup.LookLeft, LookLeftL);
	__set_curve_value(CurrentEyesState, EyeRightSetup.LookLeft, LookLeftR);
	// LookRight
	__set_curve_value(CurrentEyesState, EyeLeftSetup.LookRight, LookRightL);
	__set_curve_value(CurrentEyesState, EyeRightSetup.LookRight, LookRightR);
	// LookDown
	__set_curve_value(CurrentEyesState, EyeLeftSetup.LookDown, LookDownL);
	__set_curve_value(CurrentEyesState, EyeRightSetup.LookDown, LookDownR);
	// LookUp
	__set_curve_value(CurrentEyesState, EyeLeftSetup.LookUp, LookUpL);
	__set_curve_value(CurrentEyesState, EyeRightSetup.LookUp, LookUpR);

	if (EyeLeftSetup.bPropagateToARCurves)
	{
		__set_curve_value(CurrentEyesState, EyeLookLeftL, LookLeftL);
		__set_curve_value(CurrentEyesState, EyeLookRightL, LookRightL);
		__set_curve_value(CurrentEyesState, EyeLookDownL, LookDownL);
		__set_curve_value(CurrentEyesState, EyeLookUpL, LookUpL);
	}
	if (EyeRightSetup.bPropagateToARCurves)
	{
		__set_curve_value(CurrentEyesState, EyeLookLeftR, LookLeftR);
		__set_curve_value(CurrentEyesState, EyeLookRightR, LookRightR);
		__set_curve_value(CurrentEyesState, EyeLookDownR, LookDownR);
		__set_curve_value(CurrentEyesState, EyeLookUpR, LookUpR);
	}
}

#undef __is_anim_converted
#undef __set_anim_converted