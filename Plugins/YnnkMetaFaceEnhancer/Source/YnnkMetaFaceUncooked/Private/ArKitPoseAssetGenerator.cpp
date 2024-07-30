// (c) Yuri N. K. 2022. All rights reserved.
// ykasczc@gmail.com

#include "ArKitPoseAssetGenerator.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimationAsset.h"
#include "Animation/PoseAsset.h"
#include "LiveLinkRemapAsset.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Interfaces/IPluginManager.h"
#include "YnnkLipSyncFunctionLibrary.h"
#include "MetaFaceEditorFunctionLibrary.h"
#include "LipsyncAnimationEditorLibrary.h"
#include "AnimationBlueprintLibrary.h"
#include "Animation/AnimSequence.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimData/AnimDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "YnnkTypes.h"
#include "MetaFaceTypes.h"

#define LOCTEXT_NAMESPACE "UArKitPoseAssetGenerator"

UArKitPoseAssetGenerator::UArKitPoseAssetGenerator()
	: TargetSkeletalMesh(nullptr)
	, CurvesRemapAsset(ULiveLinkRemapAsset::StaticClass())
{
	UMetaFaceEditorFunctionLibrary::GetListOfARFacialCurves(ArKitCurveNames);

	ManualCurvesBinding.Empty();
	for (const auto& Curve : ArKitCurveNames)
	{
		ManualCurvesBinding.Add(Curve, Curve);
	}

	// default setup for CharacterCreator3/4
	// Note: for CC4 it's needed to change Facial Preset to "Traditional CC3+"
	CurveToBoneBinding.Add(TEXT("JawOpen"), FMetaFaceCurveToBone(TEXT("cc_base_jawroot"), FRotator(0.f, -45.f, 0.f)));
	CurveToBoneBinding.Add(TEXT("EyeLookInRight"), FMetaFaceCurveToBone(TEXT("cc_base_r_eye"), FRotator(0.f, -45.f, 0.f)));
	CurveToBoneBinding.Add(TEXT("EyeLookOutRight"), FMetaFaceCurveToBone(TEXT("cc_base_r_eye"), FRotator(0.f, 45.f, 0.f)));
	CurveToBoneBinding.Add(TEXT("EyeLookUpRight"), FMetaFaceCurveToBone(TEXT("cc_base_r_eye"), FRotator(0.f, 0.f, -45.f)));
	CurveToBoneBinding.Add(TEXT("EyeLookDownRight"), FMetaFaceCurveToBone(TEXT("cc_base_r_eye"), FRotator(0.f, 0.f, 45.f)));
	CurveToBoneBinding.Add(TEXT("EyeLookInLeft"), FMetaFaceCurveToBone(TEXT("cc_base_l_eye"), FRotator(0.f, 45.f, 0.f)));
	CurveToBoneBinding.Add(TEXT("EyeLookOutLeft"), FMetaFaceCurveToBone(TEXT("cc_base_l_eye"), FRotator(0.f, -45.f, 0.f)));
	CurveToBoneBinding.Add(TEXT("EyeLookUpLeft"), FMetaFaceCurveToBone(TEXT("cc_base_l_eye"), FRotator(0.f, 0.f, -45.f)));
	CurveToBoneBinding.Add(TEXT("EyeLookDownLeft"), FMetaFaceCurveToBone(TEXT("cc_base_l_eye"), FRotator(0.f, 0.f, 45.f)));
}

bool UArKitPoseAssetGenerator::GeneratePoseAsset()
{
	UMetaFaceEditorFunctionLibrary::GetListOfARFacialCurves(ArKitCurveNames);

	FText MsgTitle = FText::FromString(TEXT("Generate Pose Asset"));
	if (!IsValid(TargetSkeletalMesh))
	{
		FMessageDialog::Open(EAppMsgType::Type::Ok, FText::FromString(TEXT("You need to select TargetSkeletalMesh")), MsgTitle);
		return false;
	}

	// Initialize AnimationData
	float FrameInterval = 1.f / 30.f;
	int32 FrameNumber = 0;
	SourceAnimationData.Empty();
	for (const auto& CurveName : ArKitCurveNames)
	{
		FSimpleFloatCurve Curve;
		float TimePeak = (float)FrameNumber * FrameInterval;

		if (FrameNumber > 0)
		{
			Curve.Values.Add(FSimpleFloatValue(TimePeak - FrameInterval, 0.f));
		}
		Curve.Values.Add(FSimpleFloatValue(TimePeak, 1.f));
		Curve.Values.Add(FSimpleFloatValue(TimePeak + FrameInterval, 0.f));

		SourceAnimationData.Add(CurveName, Curve);

		FrameNumber++;
	}

	// Rename curves
	RemapCurves();

	// Create AnimSequence
	TObjectPtr<UAnimSequence> AnimAsset = GenerateAnimSequenceFromData(TEXT("_AR_Anim"));

	if (!IsValid(AnimAsset))
	{
		FMessageDialog::Open(EAppMsgType::Type::Ok, FText::FromString(TEXT("Failed to create animation sequence.")), MsgTitle);
		return false;
	}

	// Create jaw animation
	if (CurveToBoneBinding.Num() > 0.f && !bSepareteBonesAndBlends)
	{
		GenerateBonesAnimation(AnimAsset);
	}

	// Create Pose Asset from animation sequence
	FString InAssetPath = FPaths::GetPath(TargetSkeletalMesh->GetPathName());
	FString InAssetName = TargetSkeletalMesh->GetFName().ToString() + TEXT("_AR_PA");

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewObject = AssetTools.CreateAsset(
		InAssetName,
		InAssetPath,
		UPoseAsset::StaticClass(), nullptr);
	UPoseAsset* LSPoseAsset = Cast<UPoseAsset>(NewObject);

	TArray<FName> CurvesList;
	SourceAnimationData.GetKeys(CurvesList);
	TArray<FName> InputPoseNames;
	USkeleton* TargetSkeleton = __uev_access_skeleton(TargetSkeletalMesh);
	for (int32 Index = 0; Index < CurvesList.Num(); ++Index)
	{
		const FName& PoseName = CurvesList[Index];
		if (!TargetSkeleton->GetCurveMetaData(PoseName))
		{
			TargetSkeleton->AddCurveMetaData(PoseName);
		}

		// we want same names in multiple places
		InputPoseNames.AddUnique(PoseName);
	}

	LSPoseAsset->CreatePoseFromAnimation(AnimAsset, &InputPoseNames);
	LSPoseAsset->SetSkeleton(TargetSkeleton);

	// Pose assets containing bone animation work correctly only in additive mode
	if (CurveToBoneBinding.Num() > 0 && !bSepareteBonesAndBlends)
	{
		LSPoseAsset->ConvertSpace(true, INDEX_NONE);
	}

	FString szResult = TEXT("Pose Asset [") + LSPoseAsset->GetName() + TEXT("] was successfully created at ") + InAssetPath;
	FMessageDialog::Open(EAppMsgType::Type::Ok, FText::FromString(szResult), MsgTitle);

	// Step 2. Create second pose asset to drive bones only
	if (CurveToBoneBinding.Num() > 0 && bSepareteBonesAndBlends)
	{
		AnimationData.Empty();
		for (const auto& CurveName : ArKitCurveNames)
		{
			AnimationData.Add(CurveName);
		}

		// Create AnimSequence
		TObjectPtr<UAnimSequence> AnimAssetBonesOnly = GenerateAnimSequenceFromData(TEXT("_AR_BonesOnly_Anim"));
		GenerateBonesAnimation(AnimAssetBonesOnly);

		// Create Pose Asset from animation sequence
		InAssetPath = FPaths::GetPath(TargetSkeletalMesh->GetPathName());
		InAssetName = TargetSkeletalMesh->GetFName().ToString() + TEXT("_AR_BonesOnly_PA");

		UObject* NewPAObject = AssetTools.CreateAsset(
			InAssetName,
			InAssetPath,
			UPoseAsset::StaticClass(), nullptr);
		UPoseAsset* BOPoseAsset = Cast<UPoseAsset>(NewPAObject);

		BOPoseAsset->CreatePoseFromAnimation(AnimAssetBonesOnly, &InputPoseNames);
		BOPoseAsset->SetSkeleton(TargetSkeleton);
		BOPoseAsset->ConvertSpace(true, INDEX_NONE);
	}

	return true;
}

void UArKitPoseAssetGenerator::RemapCurves()
{
	bool bUseAsset = (CurvesRemapAsset != nullptr && CurvesRemapAsset != ULiveLinkRemapAsset::StaticClass());

	ULiveLinkRemapAsset* RemapAsset = nullptr;
	if (bUseAsset)
	{
		RemapAsset = NewObject<ULiveLinkRemapAsset>(this, CurvesRemapAsset);
		RemapAsset->Initialize();
	}

	AnimationData.Empty();
	for (const auto& Curve : SourceAnimationData)
	{
		FName TargetCurveName;
		if (bUseAsset)
		{
			TargetCurveName = GetRemappedCurveName(RemapAsset, Curve.Key);
		}
		else
		{
			if (FName* NewNamePtr = ManualCurvesBinding.Find(Curve.Key))
			{
				TargetCurveName = *NewNamePtr;
			}
			else TargetCurveName = Curve.Key;
		}

		if (!TargetCurveName.IsNone())
		{
			AnimationData.Add(TargetCurveName, Curve.Value);
		}
	}

	if (bUseAsset)
	{
		__uev_destory_object(RemapAsset);
	}
}

TObjectPtr<UAnimSequence> UArKitPoseAssetGenerator::GenerateAnimSequenceFromData(FString Postfix) const
{
	// Create new asset
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	FString InAssetPath = FPaths::GetPath(TargetSkeletalMesh->GetPathName());
	FString InAssetName = TargetSkeletalMesh->GetFName().ToString() + Postfix;
	UObject* NewObject = AssetTools.CreateAsset(
		InAssetName,
		InAssetPath,
		UAnimSequence::StaticClass(), nullptr);

	UAnimSequence* NewSeq = Cast<UAnimSequence>(NewObject);
	if (!NewSeq)
	{
		return nullptr;
	}

	// Initialize
	const int32 UseFrameRate = 30;
	float FrameInterval = 1.f / (float)UseFrameRate;

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(NewSeq);

	// set skeleton
	NewSeq->ResetAnimation();
	NewSeq->SetPreviewSkeletalMesh(TargetSkeletalMesh);
	NewSeq->SetSkeleton(__uev_access_skeleton(TargetSkeletalMesh));
	NewSeq->ImportFileFramerate = (float)UseFrameRate;
	NewSeq->ImportResampleFramerate = UseFrameRate;
	// Initialize new asset
	NewSeq->GetController().OpenBracket(LOCTEXT("InitializeAnimation", "Initialize New Anim Sequence"));
	{
#if ENGINE_MINOR_VERSION > 1
		NewSeq->GetController().InitializeModel();
#endif
		float Duration = FrameInterval * (ArKitCurveNames.Num() - 1);
		NewSeq->GetController().SetFrameRate(FFrameRate(UseFrameRate, 1), true);
#if ENGINE_MINOR_VERSION > 1
		NewSeq->GetController().SetNumberOfFrames(NewSeq->GetController().ConvertSecondsToFrameNumber(Duration), true);
#else
		NewSeq->GetController().SetPlayLength(Duration, true);
#endif
		NewSeq->GetController().NotifyPopulated();
	}
	NewSeq->GetController().CloseBracket();

	// Fill curves
	ULipsyncAnimationEditorLibrary::SaveCurvesInAnimSequence(NewSeq, AnimationData);

	return NewSeq;
}

void UArKitPoseAssetGenerator::GenerateBonesAnimation(TObjectPtr<UAnimSequence> AnimSequence) const
{	
	// ref skeleton of skeletal mesh is preferreable to ref skeleton from USkeleton
	const FReferenceSkeleton& RefSkeleton = __uev_access_refskeleton(TargetSkeletalMesh);

	TMap<FName, FTransform> BoneRefTransforms;
	for (const auto& BoneBinding : CurveToBoneBinding)
	{
		const FName& BoneName = BoneBinding.Value.BoneName;
		if (BoneRefTransforms.Contains(BoneName)) continue;

		int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			FMessageDialog::Open(EAppMsgType::Type::Ok, FText::FromString(TEXT("Invalid name of bone")));
			return;
		}
		if (!ArKitCurveNames.Contains(BoneBinding.Key))
		{
			FMessageDialog::Open(EAppMsgType::Type::Ok, FText::FromString(TEXT("Invalid animation data. Can't find curve name.")));
			return;
		}

		BoneRefTransforms.Add(BoneName, RefSkeleton.GetRefBonePose()[BoneIndex]);
	}

	// Bone/track name -> list of rotators from FMetaFaceCurveToBone
	// inversed data: saving curve name to "BoneName"
	TMap<FName, TArray<FMetaFaceCurveToBone>> TrackToCurveMap;
	for (const auto& Binding : CurveToBoneBinding)
	{
		if (TrackToCurveMap.Contains(Binding.Value.BoneName))
		{
			TrackToCurveMap[Binding.Value.BoneName].Add(FMetaFaceCurveToBone(Binding.Key, Binding.Value.Orientation));
		}
		else
		{			
			TrackToCurveMap.Add(Binding.Value.BoneName, { FMetaFaceCurveToBone(Binding.Key, Binding.Value.Orientation) });
		}
	}
	
	// Keys in animation
	int32 KeysNum = AnimSequence->GetDataModel()->GetNumberOfKeys();

	// Create map with animation frames
	// Key is bone name
	TMap<FName, FRawAnimSequenceTrack> OutBoneTracks;
	for (const auto& BoneBinding : TrackToCurveMap)
	{
		const FName TrackName = BoneBinding.Key;
		OutBoneTracks.Add(TrackName);
		OutBoneTracks[TrackName].PosKeys.SetNumUninitialized(KeysNum);
		OutBoneTracks[TrackName].RotKeys.SetNumUninitialized(KeysNum);
		OutBoneTracks[TrackName].ScaleKeys.SetNumUninitialized(KeysNum);
	}

	// Generate animation keys
	for (int32 FrameIndex = 0; FrameIndex < KeysNum; FrameIndex++)
	{
		float Time;
		UAnimationBlueprintLibrary::GetTimeAtFrame(AnimSequence, FrameIndex, Time);

		for (auto& Track : OutBoneTracks)
		{
			const FName TrackName = Track.Key;
			float Pitch = 0.f, Yaw = 0.f, Roll = 0.f;

			// iterate all curves affecting this track and summ their rotators with corresponding multipliers
			for (const auto& CurveData : TrackToCurveMap[TrackName])
			{
				// note: we saved CurveName in BoneName
				const double Alpha = SourceAnimationData[CurveData.BoneName].GetValueAtTime(Time);
				Pitch += CurveData.Orientation.Pitch * Alpha;
				Yaw += CurveData.Orientation.Yaw * Alpha;
				Roll += CurveData.Orientation.Roll * Alpha;
			}

			const FTransform RefTransform = BoneRefTransforms[TrackName];

			FRotator TargetRot = FRotator(Pitch, Yaw, Roll).GetNormalized();
			FTransform NewBonePose = FTransform(TargetRot) * RefTransform;

			// Update rotation
			Track.Value.PosKeys[FrameIndex] = (FVector3f)RefTransform.GetTranslation();
			Track.Value.RotKeys[FrameIndex] = (FQuat4f)NewBonePose.GetRotation();
			Track.Value.ScaleKeys[FrameIndex] = (FVector3f)RefTransform.GetScale3D();
		}
	}
	
	// Save bone tracks to animation sequence
	IAnimationDataController& Controller = AnimSequence->GetController();
	Controller.OpenBracket(FText::FromString(TEXT("Generate Bones Animation")));
	{
		for (auto& Track : OutBoneTracks)
		{
			const FName BoneName = Track.Key;
			Controller.RemoveBoneTrack(BoneName);
#if ENGINE_MINOR_VERSION > 1
			Controller.AddBoneCurve(BoneName);
#else
			Controller.AddBoneTrack(BoneName);
#endif
			Controller.SetBoneTrackKeys(BoneName, Track.Value.PosKeys, Track.Value.RotKeys, Track.Value.ScaleKeys);
		}
	}
	Controller.CloseBracket();
}

FName UArKitPoseAssetGenerator::GetRemappedCurveName(ULiveLinkRemapAsset* RemapAsset, const FName& InCurveName) const
{
	UFunction* Func_GetRemappedCurveName = RemapAsset->GetClass()->FindFunctionByName(FName("GetRemappedCurveName"));
	if (!Func_GetRemappedCurveName)
	{
		return NAME_None;
	}

	FStructOnScope FuncParam(Func_GetRemappedCurveName);

	// Set input properties
	FNameProperty* InProp = CastField<FNameProperty>(Func_GetRemappedCurveName->FindPropertyByName(TEXT("CurveName")));
	if (!InProp)
	{
		return NAME_None;
	}
	InProp->SetPropertyValue_InContainer(FuncParam.GetStructMemory(), InCurveName);

	// Call function
	RemapAsset->ProcessEvent(Func_GetRemappedCurveName, FuncParam.GetStructMemory());

	// Get return property
	FNameProperty* OutProp = CastField<FNameProperty>(Func_GetRemappedCurveName->GetReturnProperty());
	if (!OutProp)
	{
		return NAME_None;
	}

	return OutProp->GetPropertyValue_InContainer(FuncParam.GetStructMemory());
}

#undef LOCTEXT_NAMESPACE