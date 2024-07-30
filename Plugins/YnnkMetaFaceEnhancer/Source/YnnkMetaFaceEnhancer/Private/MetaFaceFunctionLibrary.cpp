// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

#include "MetaFaceFunctionLibrary.h"
#include "YnnkLipSyncFunctionLibrary.h"
#include "MetaFaceTypes.h"
#include "YnnkVoiceLipsyncData.h"
#include "Animation/PoseAsset.h"
#include "Misc/FileHelper.h"
#include "Misc/CString.h"
#include "HAL/PlatformApplicationMisc.h"
#include "YnnkMetaFaceEnhancer.h"
#include "YnnkMetaFaceController.h"
#include "AsyncAnimBuilder.h"
#include "YnnkMetaFaceSettings.h"
#include "NeuralProcessWrapper.h"
#include "Async/Async.h"

#define __is_anim_converted(animation) (animation.AnimationFlag && 1)
#define __set_anim_converted(animation) animation.AnimationFlag = 1

FString UMFFunctionLibrary::CopyFacialFrameToClipboard(const TMap<FName, float>& InData, const TArray<FString>& FilterCurves)
{
	FString out;
	if (FilterCurves.Num() > 0)
	{
		for (const auto& p : FilterCurves)
		{
			if (const float* value = InData.Find(*p))
			{
				if (out.Len() > 0) out.Append(TEXT(","));
				out.Append(TEXT("(") + p + TEXT(",") + FString::SanitizeFloat(*value) + TEXT(")"));
			}
		}
	}
	else
	{
		for (const auto& p : InData)
		{
			if (out.Len() > 0) out.Append(TEXT(","));
			out.Append(TEXT("(") + p.Key.ToString() + TEXT(",") + FString::SanitizeFloat(p.Value) + TEXT(")"));
		}
	}

	out = TEXT("(") + out + TEXT(")");
	FPlatformApplicationMisc::ClipboardCopy(*out);
	return out;
}

void UMFFunctionLibrary::CreateMetaFaceAnimationCurves(UYnnkVoiceLipsyncData* LipsyncData, bool bCreateLipSync, bool bCreateFacialAnimation, const FMetaFaceGenerationSettings& MetaFaceSettings)
{
	auto ModuleMFE = FModuleManager::GetModulePtr<FYnnkMetaFaceEnhancerModule>(TEXT("YnnkMetaFaceEnhancer"));

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
				UMFFunctionLibrary::RawDataToLipsync(LipsyncData, RawData, AnimationData, MetaFaceSettings);

				const FName FrownL = TEXT("MouthFrownLeft");
				const FName FrownR = TEXT("MouthFrownRight");
				const FName SmileL = TEXT("MouthSmileLeft");
				const FName SmileR = TEXT("MouthSmileRight");

				if (MetaFaceSettings.bBalanceSmileFrownCurves
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

				if (MetaFaceSettings.bLipSyncToSkeletonCurves)
				{
					UMFFunctionLibrary::ConvertFacialAnimCurves(AnimationData, MetaFaceSettings.ArKitCurvesPoseAsset);
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
				UMFFunctionLibrary::RawDataToFacialAnimation(LipsyncData, RawData, AnimationData, MetaFaceSettings);
				if (MetaFaceSettings.bBalanceSmileFrownCurves)
				{
					AnimationData.Remove(TEXT("MouthSmileLeft"));
					AnimationData.Remove(TEXT("MouthSmileRight"));
				}
				if (MetaFaceSettings.bFacialAnimationToSkeletonCurves)
				{
					UMFFunctionLibrary::ConvertFacialAnimCurves(AnimationData, MetaFaceSettings.ArKitCurvesPoseAsset);
				}

				const float FacialAnimationPauseDuration = 1.;
				FacialAnimation.Initialize(AnimationData, true, FacialAnimationPauseDuration, FacialAnimationPauseDuration * 0.5f - 0.01f);
			}
		}
	}

	// Create animations preset
	FFacialAnimCollection NewItem;
	NewItem.LipSync = LipsyncAnimation;
	NewItem.FacialAnimation = FacialAnimation;

	//LipsyncData->

	if (MetaFaceSettings.bLipSyncToSkeletonCurves && !__is_anim_converted(LipsyncAnimation))
	{
		auto AnimCopy = LipsyncAnimation.AnimationData;
		UMFFunctionLibrary::ConvertFacialAnimCurves(AnimCopy, MetaFaceSettings.ArKitCurvesPoseAsset);
		NewItem.LipSync.AnimationData.Append(AnimCopy);
		__set_anim_converted(LipsyncAnimation);
	}

	if (MetaFaceSettings.bFacialAnimationToSkeletonCurves && !__is_anim_converted(FacialAnimation))
	{
		auto AnimCopy = FacialAnimation.AnimationData;
		UMFFunctionLibrary::ConvertFacialAnimCurves(AnimCopy, MetaFaceSettings.ArKitCurvesPoseAsset);
		NewItem.FacialAnimation.AnimationData.Append(AnimCopy);
		__set_anim_converted(FacialAnimation);
	}
	
#if WITH_EDITOR
	LipsyncData->Modify();
#endif
	LipsyncData->ExtraAnimData1 = NewItem.LipSync.AnimationData;
	LipsyncData->ExtraAnimData2 = NewItem.FacialAnimation.AnimationData;

}

void UMFFunctionLibrary::FacialAnimation_Initialize(FMHFacialAnimation& Animation, const TMap<FName, FSimpleFloatCurve>& InAnimationData, float Intensity, bool bInFadeOnPause, float InFadePauseDuration, float InFadeTime)
{
	Animation.Initialize(InAnimationData, bInFadeOnPause, InFadePauseDuration, InFadeTime);
	Animation.Intensity = Intensity;
}

void UMFFunctionLibrary::FacialAnimation_Play(FMHFacialAnimation& Animation)
{
	Animation.Play();
}

void UMFFunctionLibrary::FacialAnimation_Stop(FMHFacialAnimation& Animation)
{
	Animation.Stop();
}

void UMFFunctionLibrary::FacialAnimation_Tick(FMHFacialAnimation& Animation, float PlayTime, UYnnkLipsyncController* LipsyncController)
{
	Animation.ProcessFrame(PlayTime, LipsyncController);
}

bool UMFFunctionLibrary::BuildFacialAnimationData(UYnnkVoiceLipsyncData* LipsyncData, bool bCreateLipSync, bool bCreateFacialAnimation, const FAsyncFacialAnimationResult& CallbackEvent)
{
	if (!LipsyncData || LipsyncData->PhonemesData.Num() == 0)
	{
		return false;
	}

	UAsyncAnimBuilder* Builder = UAsyncAnimBuilder::CreateAsyncAnimBuilder(LipsyncData, bCreateLipSync, bCreateFacialAnimation, CallbackEvent);
	if (Builder)
	{
		Builder->Start();
		return true;
	}
	return false;
}

float UMFFunctionLibrary::FacialAnimation_CurveValueAtTime(FMHFacialAnimation& Animation, FName Curve, float PlayTime)
{
	if (const auto CurveData = Animation.AnimationData.Find(Curve))
	{
		return CurveData->GetValueAtTime(PlayTime);
	}
	else
	{
		return -1.f;
	}
}

FString UMFFunctionLibrary::FacialAnimation_GetDescription(FMHFacialAnimation& Animation)
{
	return Animation.GetDescription();
}

UNeuralProcessWrapper* UMFFunctionLibrary::InitializeYnnkMetaFace(UObject* Parent)
{
	UNeuralProcessWrapper* p = IsValid(Parent)
		? NewObject<UNeuralProcessWrapper>(Parent)
		: NewObject<UNeuralProcessWrapper>();

	auto ModuleMFE = FModuleManager::GetModulePtr<FYnnkMetaFaceEnhancerModule>(TEXT("YnnkMetaFaceEnhancer"));
	if (ModuleMFE)
	{
		ModuleMFE->InitializeTorchModels(p);
	}
	return p;
}

void UMFFunctionLibrary::PrepareYnnkMetaFaceModel()
{
	// Start import thread
	Async(EAsyncExecution::Thread, []()
	{
		FString OutData;
		bool bResult = false;

		auto ModuleMFE = FModuleManager::GetModulePtr<FYnnkMetaFaceEnhancerModule>(TEXT("YnnkMetaFaceEnhancer"));

		if (ModuleMFE)
		{
			TArray<FPhonemeTextData> PhonemesData;
			PhonemesData.Add(FPhonemeTextData(0.330000f, TEXT('d'), true));
			PhonemesData.Add(FPhonemeTextData(0.360000f, TEXT('a'), false));
			PhonemesData.Add(FPhonemeTextData(0.517500f, TEXT('l'), true));
			PhonemesData.Add(FPhonemeTextData(0.615000f, TEXT('e'), false));
			PhonemesData.Add(FPhonemeTextData(0.680000f, TEXT('s'), false));
			PhonemesData.Add(FPhonemeTextData(0.745000f, TEXT('t'), false));
			PhonemesData.Add(FPhonemeTextData(0.866667f, TEXT('p'), true));
			PhonemesData.Add(FPhonemeTextData(0.923333f, TEXT('r'), false));
			PhonemesData.Add(FPhonemeTextData(0.980000f, TEXT('a'), false));
			PhonemesData.Add(FPhonemeTextData(1.036667f, TEXT('b'), false));
			PhonemesData.Add(FPhonemeTextData(1.093333f, TEXT('l'), false));
			PhonemesData.Add(FPhonemeTextData(1.150000f, TEXT('a'), false));
			PhonemesData.Add(FPhonemeTextData(1.235000f, TEXT('m'), false));
			PhonemesData.Add(FPhonemeTextData(1.391250f, TEXT('h'), true));
			PhonemesData.Add(FPhonemeTextData(1.462500f, TEXT('o'), false));
			PhonemesData.Add(FPhonemeTextData(1.510000f, TEXT('p'), false));
			PhonemesData.Add(FPhonemeTextData(1.557500f, TEXT('f'), false));
			PhonemesData.Add(FPhonemeTextData(1.605000f, TEXT('a'), false));
			PhonemesData.Add(FPhonemeTextData(1.676250f, TEXT('l'), false));
			PhonemesData.Add(FPhonemeTextData(1.747500f, TEXT('i'), false));
			PhonemesData.Add(FPhonemeTextData(1.980000f, TEXT('i'), true));
			PhonemesData.Add(FPhonemeTextData(2.025000f, TEXT('z'), false));
			PhonemesData.Add(FPhonemeTextData(2.150000f, TEXT('u'), true));
			PhonemesData.Add(FPhonemeTextData(2.230000f, TEXT('e'), false));
			PhonemesData.Add(FPhonemeTextData(2.270000f, TEXT('n'), false));
			PhonemesData.Add(FPhonemeTextData(2.355000f, TEXT('a'), true));
			PhonemesData.Add(FPhonemeTextData(2.377500f, TEXT('i'), false));
			PhonemesData.Add(FPhonemeTextData(2.454250f, TEXT('b'), true));
			PhonemesData.Add(FPhonemeTextData(2.508500f, TEXT('i'), false));
			PhonemesData.Add(FPhonemeTextData(2.562749f, TEXT('g'), false));
			PhonemesData.Add(FPhonemeTextData(2.616999f, TEXT('i'), false));
			PhonemesData.Add(FPhonemeTextData(2.671249f, TEXT('n'), false));
			PhonemesData.Add(FPhonemeTextData(2.749124f, TEXT('d'), true));
			PhonemesData.Add(FPhonemeTextData(2.772749f, TEXT('a'), false));
			PhonemesData.Add(FPhonemeTextData(2.905000f, TEXT('p'), true));
			PhonemesData.Add(FPhonemeTextData(2.990000f, TEXT('e'), false));
			PhonemesData.Add(FPhonemeTextData(3.075000f, TEXT('k'), false));
			PhonemesData.Add(FPhonemeTextData(3.160000f, TEXT('a'), false));
			PhonemesData.Add(FPhonemeTextData(3.245000f, TEXT('j'), false));
			PhonemesData.Add(FPhonemeTextData(3.406667f, TEXT('p'), true));
			PhonemesData.Add(FPhonemeTextData(3.483333f, TEXT('r'), false));
			PhonemesData.Add(FPhonemeTextData(3.560000f, TEXT('a'), false));
			PhonemesData.Add(FPhonemeTextData(3.675000f, TEXT('s'), false));
			PhonemesData.Add(FPhonemeTextData(3.790000f, TEXT('e'), false));
			PhonemesData.Add(FPhonemeTextData(3.905000f, TEXT('s'), false));
			PhonemesData.Add(FPhonemeTextData(4.180000f, TEXT('e'), true));
			PhonemesData.Add(FPhonemeTextData(4.223334f, TEXT('f'), false));
			PhonemesData.Add(FPhonemeTextData(4.266667f, TEXT('t'), false));
			PhonemesData.Add(FPhonemeTextData(4.310001f, TEXT('o'), false));
			PhonemesData.Add(FPhonemeTextData(4.470000f, TEXT('e'), true));
			PhonemesData.Add(FPhonemeTextData(4.545000f, TEXT('f'), true));
			PhonemesData.Add(FPhonemeTextData(4.590000f, TEXT('i'), false));
			PhonemesData.Add(FPhonemeTextData(4.680000f, TEXT('u'), false));
			PhonemesData.Add(FPhonemeTextData(4.860000f, TEXT('s'), true));
			PhonemesData.Add(FPhonemeTextData(4.950000f, TEXT('e'), false));
			PhonemesData.Add(FPhonemeTextData(5.040000f, TEXT('k'), false));
			PhonemesData.Add(FPhonemeTextData(5.130001f, TEXT('a'), false));
			PhonemesData.Add(FPhonemeTextData(5.175001f, TEXT('n'), false));
			PhonemesData.Add(FPhonemeTextData(5.220001f, TEXT('d'), false));
			PhonemesData.Add(FPhonemeTextData(5.265001f, TEXT('z'), false));
			PhonemesData.Add(FPhonemeTextData(5.385000f, TEXT('i'), true));
			PhonemesData.Add(FPhonemeTextData(5.422500f, TEXT('t'), false));
			PhonemesData.Add(FPhonemeTextData(5.550000f, TEXT('1'), true));
			PhonemesData.Add(FPhonemeTextData(5.640000f, TEXT('o'), false));
			PhonemesData.Add(FPhonemeTextData(5.730000f, TEXT('z'), false));
			PhonemesData.Add(FPhonemeTextData(5.865000f, TEXT('m'), true));
			PhonemesData.Add(FPhonemeTextData(5.910000f, TEXT('i'), false));
			PhonemesData.Add(FPhonemeTextData(6.075000f, TEXT('e'), true));
			PhonemesData.Add(FPhonemeTextData(6.112500f, TEXT('n'), false));
			PhonemesData.Add(FPhonemeTextData(6.330000f, TEXT('e'), true));
			PhonemesData.Add(FPhonemeTextData(6.420000f, TEXT('r'), false));
			PhonemesData.Add(FPhonemeTextData(6.510000f, TEXT('o'), false));

			UNeuralProcessWrapper* NeuralProcessor = ModuleMFE->GetNeuralProcessor();

			if (IsValid(NeuralProcessor))
			{
				RawAnimDataMap GeneratedData;

				// Lip-sync
				NeuralProcessor->ProcessPhonemesData(PhonemesData, true, GeneratedData);
				GeneratedData.Empty();

				// Facial animation
				NeuralProcessor->ProcessPhonemesData2(PhonemesData, false, GeneratedData);
			}
		}
		else
		{
			OutData = TEXT("ERROR: can't get pointer to module");
		}
	});
}

FRotator UMFFunctionLibrary::MakeHeadRotatorFromAnimFrame(const TMap<FName, float>& AnimationFrame, float OffsetRoll, float OffsetPitch, float OffsetYaw)
{
	const float* Roll = AnimationFrame.Find(TEXT("HeadRoll"));
	const float* Pitch = AnimationFrame.Find(TEXT("HeadPitch"));
	const float* Yaw = AnimationFrame.Find(TEXT("HeadYaw"));
	
	return (Roll && Pitch && Yaw)
		? FRotator(*Roll * 50.f + OffsetRoll, *Pitch * -50.f + OffsetPitch, *Yaw * 50.f + OffsetYaw)
		: FRotator::ZeroRotator;
}

void UMFFunctionLibrary::RawDataToLipsync(const UYnnkVoiceLipsyncData* PhonemesSource, const RawAnimDataMap& InData, TMap<FName, FSimpleFloatCurve>& OutAnimationCurves, const FMetaFaceGenerationSettings& MetaFaceSettings)
{
	const int32 PhonemesNum = PhonemesSource->PhonemesData.Num();
	float PlayTime = PhonemesSource->PhonemesData.Last().Time + 0.05f;
	float PreviousPhonemeTime = 0.f;

	// Debug output
	/*
	for (const auto& Curve : InData)
	{
		UE_LOG(LogTemp, Log, TEXT("Curve: %s has %d points"), *Curve.Key.ToString(), Curve.Value.Num());

		FString Values;
		for (const auto& Val : Curve.Value)
		{
			Values.Append(FString::SanitizeFloat(Val));
			Values.Append(TEXT(" "));
		}
		Values.TrimEndInline();
		UE_LOG(LogTemp, Log, TEXT("%s"), *Values);
	}
	*/

	auto Settings = GetDefault<UYnnkMetaFaceSettings>();
	// Due to complifications with CC4 characters it's better not to convert lip-sync here and do it later in YnnkMetaFaceController
	// (to keep original curves controlling bones)
	float VisemeApplyAlpha = MetaFaceSettings.VisemeApplyAlpha;
	float LipsyncNeuralIntensity = MetaFaceSettings.LipsyncNeuralIntensity;
	float LipsyncSmoothness = MetaFaceSettings.LipsyncSmoothness;
	UPoseAsset* ArKitCurvesPoseAsset = MetaFaceSettings.ArKitCurvesPoseAsset;
	bool bConvertLipSync = false;// MetaFaceSettings.bLipSyncToSkeletonCurves;

	// initialize out data
	OutAnimationCurves.Empty();
	for (const auto& Curve : InData)
	{
		OutAnimationCurves.Add(Curve.Key);
	}

	// fill out data
	for (int32 Index = 0; Index < PhonemesNum; ++Index)
	{
		const auto& Phoneme = PhonemesSource->PhonemesData[Index];

		EYnnkViseme v = YnnkHelpers::SymbolToViseme(Phoneme.Symbol[0]);

		float TimeFadeIn = 0.f, TimeFadeOut = 0.f;
		bool bNewWordStarted = Phoneme.bWordStart;
		if (bNewWordStarted)
		{
			if (Phoneme.Time - PreviousPhonemeTime < 0.2f)
			{
				bNewWordStarted = false;
			}
			else
			{
				float OffsetBetweenWords = FMath::Min(0.2f, (Phoneme.Time - PreviousPhonemeTime) * 0.5f - 0.05f);
				TimeFadeIn = PreviousPhonemeTime + OffsetBetweenWords;
				TimeFadeOut = Phoneme.Time - OffsetBetweenWords;
			}
		}

		for (const auto& Curve : InData)
		{
			const FName& CurveName = Curve.Key;

			// add fade in-out
			if (bNewWordStarted)
			{
				if (PreviousPhonemeTime == 0.f)
				{
					OutAnimationCurves[CurveName].Values.Add(FSimpleFloatValue(0.f, 0.f, 1));
				}
				else
				{
					OutAnimationCurves[CurveName].Values.Add(FSimpleFloatValue(TimeFadeIn, 0.f));
					OutAnimationCurves[CurveName].Values.Add(FSimpleFloatValue(TimeFadeOut, 0.f));
				}
			}

			float WordPlaceMultiplier = 1.f;
			if (Phoneme.bWordStart)
			{
				WordPlaceMultiplier = 0.5f;
			}
			else if (Index + 1 < PhonemesSource->PhonemesData.Num() && PhonemesSource->PhonemesData[Index + 1].bWordStart)
			{
				WordPlaceMultiplier = 0.5f;
			}

			// get NN value
			if (!InData[CurveName].IsValidIndex(Index))
			{
				continue;
			}

			float val = InData[CurveName][Index];
			val *= WordPlaceMultiplier;

			// manual smooth
			const EYnnkViseme PrevViseme = (Index > 0)
				? YnnkHelpers::SymbolToViseme(PhonemesSource->PhonemesData[Index - 1].Symbol[0])
				: EYnnkViseme::YV_Max;
			const EYnnkViseme NextViseme = (Index + 1 < PhonemesSource->PhonemesData.Num())
				? YnnkHelpers::SymbolToViseme(PhonemesSource->PhonemesData[Index + 1].Symbol[0])
				: EYnnkViseme::YV_Max;
			if ((PrevViseme == EYnnkViseme::YV_BMP || PrevViseme == EYnnkViseme::YV_FV || PrevViseme == EYnnkViseme::YV_Oh || PrevViseme == EYnnkViseme::YV_WU)
				&& (NextViseme == EYnnkViseme::YV_BMP || NextViseme == EYnnkViseme::YV_FV || NextViseme == EYnnkViseme::YV_Oh || NextViseme == EYnnkViseme::YV_WU)
				&& Settings->LipsyncVisemesPreset.Contains(PrevViseme) && Settings->LipsyncVisemesPreset.Contains(NextViseme))
			{
				const float AlphaPrev = Settings->LipsyncVisemesPreset[PrevViseme].Curves[CurveName];
				const float AlphaNext = Settings->LipsyncVisemesPreset[NextViseme].Curves[CurveName];

				val = (AlphaPrev + val + AlphaNext) / 3.f;
			}
			else if ((PrevViseme == EYnnkViseme::YV_BMP || PrevViseme == EYnnkViseme::YV_FV || PrevViseme == EYnnkViseme::YV_Oh || PrevViseme == EYnnkViseme::YV_WU) && Settings->LipsyncVisemesPreset.Contains(PrevViseme))
			{
				const float AlphaPrev = Settings->LipsyncVisemesPreset[PrevViseme].Curves[CurveName];
				val = (AlphaPrev + val) / 2.f;
			}
			else if ((NextViseme == EYnnkViseme::YV_BMP || NextViseme == EYnnkViseme::YV_FV || NextViseme == EYnnkViseme::YV_Oh || NextViseme == EYnnkViseme::YV_WU) && Settings->LipsyncVisemesPreset.Contains(NextViseme))
			{
				const float AlphaNext = Settings->LipsyncVisemesPreset[NextViseme].Curves[CurveName];
				val = (val + AlphaNext) / 2.f;
			}

			int32 Flag = 0;
			if (const FMetaFacePose* Src = Settings->LipsyncVisemesPreset.Find(v))
			{
				if (const float* VisemeValue = Src->Curves.Find(CurveName))
				{
					float ApplyVisemeValue = *VisemeValue * VisemeApplyAlpha;
					switch (v)
					{
					case EYnnkViseme::YV_BMP:
						val = ApplyVisemeValue; Flag = 1; break;
					case EYnnkViseme::YV_WU:
						Flag = 1;
						//case EYnnkViseme::YV_FV:
					case EYnnkViseme::YV_Oh:
						val = FMath::Lerp(ApplyVisemeValue, val, LipsyncNeuralIntensity * 0.4f); break;
					default:
						val = FMath::Lerp(ApplyVisemeValue, val, LipsyncNeuralIntensity); break;
					}
				}
			}

			// fade out
			if (PlayTime - Phoneme.Time < 0.25f)
			{
				float mul = (PlayTime - Phoneme.Time) / 0.25f;
				val *= mul;
			}

			// save
			OutAnimationCurves[CurveName].Values.Add(FSimpleFloatValue(Phoneme.Time, val, Flag));
		}

		PreviousPhonemeTime = Phoneme.Time;
	}

	// reset all curves at the end
	PreviousPhonemeTime += 0.2f;
	for (auto& Curve : OutAnimationCurves)
	{
		Curve.Value.Values.Add(FSimpleFloatValue(PreviousPhonemeTime, 0.f, 1));
	}

	// apply smoothness
	if (LipsyncSmoothness > 0.f)
	{
		// minimal smooth
		for (auto& Curve : OutAnimationCurves)
		{
			auto& Points = Curve.Value.Values;
			int32 Num = Points.Num();
			for (int32 n = 0; n < 2; n++)
			{
				for (int32 i = 2; i < Num - 2; i++)
				{
					float NewVal = (Points[i - 1].Value + Points[i].Value + Points[i + 1].Value) / 3.f;
					if (Points[i].Flag & CURVEFLAG_RICH)
					{
						Points[i].Value = FMath::Lerp(Points[i].Value, NewVal, LipsyncSmoothness * 0.15f);
					}
					else
					{
						Points[i].Value = FMath::Lerp(Points[i].Value, NewVal, LipsyncSmoothness);
					}
				}
			}
		}
	}

	// convert to skeleton curves
	if (bConvertLipSync && IsValid(ArKitCurvesPoseAsset))
	{
		TMap<FName, FSimpleFloatCurve> SwapData;
		UYnnkLipSyncFunctionLibrary::ExpandPoseAnimationToCurves(OutAnimationCurves, ArKitCurvesPoseAsset, SwapData);
		OutAnimationCurves = SwapData;
	}
}

void UMFFunctionLibrary::RawDataToFacialAnimation(const UYnnkVoiceLipsyncData* PhonemesSource, const RawAnimDataMap& InData, TMap<FName, FSimpleFloatCurve>& OutAnimationCurves,
	const FMetaFaceGenerationSettings& MetaFaceSettings)
{
	if (!IsValid(PhonemesSource))
	{
		return;
	}

	const int32 PhonemesNum = PhonemesSource->PhonemesData.Num();
	float PlayTime = PhonemesSource->PhonemesData.Last().Time + 0.05f;
	float PreviousPhonemeTime = 0.f;

	auto Settings = GetDefault<UYnnkMetaFaceSettings>();
	float FacialAnimationSmoothness = MetaFaceSettings.FacialAnimationSmoothness;
	bool bFacialAnimationToSkeletonCurves = MetaFaceSettings.bFacialAnimationToSkeletonCurves;

	bool bMirrorRightBlink = false;
	if (InData.Contains(TEXT("EyeBlinkLeft")) && !InData.Contains(TEXT("EyeBlinkRight")))
	{
		bMirrorRightBlink = true;
	}

	// initialize out data
	OutAnimationCurves.Empty();
	for (const auto& Curve : InData)
	{
		OutAnimationCurves.Add(Curve.Key);
	}
	if (bMirrorRightBlink)
	{
		OutAnimationCurves.Add(TEXT("EyeBlinkRight"));
	}

	// fill out data
	for (int32 Index = 0; Index < PhonemesNum; ++Index)
	{
		const auto& Phoneme = PhonemesSource->PhonemesData[Index];
		EYnnkViseme v = PhonemesSource->SymbolToViseme(Phoneme.Symbol[0]);

		for (const auto& Curve : InData)
		{
			const FName& CurveName = Curve.Key;

			// get NN value
			if (!InData[CurveName].IsValidIndex(Index))
			{
				continue;
			}
			float val = InData[CurveName][Index];

			if (CurveName.ToString().Left(4) == TEXT("Brow"))
			{
				val *= 0.6f;
			}
			else if (CurveName.ToString().Left(9) == TEXT("EyeSquint"))
			{
				val *= 0.75f;
			}

			// fade out
			if (PlayTime - Phoneme.Time < 0.25f)
			{
				float mul = (PlayTime - Phoneme.Time) / 0.25f;
				val *= mul;
			}

			// save
			OutAnimationCurves[CurveName].Values.Add(FSimpleFloatValue(Phoneme.Time, val));

			FString szCurveName = CurveName.ToString();
			if (bMirrorRightBlink && szCurveName == TEXT("EyeBlinkLeft"))
			{
				OutAnimationCurves[TEXT("EyeBlinkRight")].Values.Add(FSimpleFloatValue(Phoneme.Time, val));
			}
		}

		PreviousPhonemeTime = Phoneme.Time;
	}

	// reset all curves at the end
	PreviousPhonemeTime += 0.8f;
	for (auto& Curve : OutAnimationCurves)
	{
		Curve.Value.Values.Add(FSimpleFloatValue(PreviousPhonemeTime, 0.f, 1));
	}

	// @TODO: create eye blink animation

	// apply smoothness
	if (FacialAnimationSmoothness > 0.f)
	{
		// smooth
		int SmoothIterations = 4;

		for (int32 cnt = 0; cnt < SmoothIterations; ++cnt)
		{
			for (auto& Curve : OutAnimationCurves)
			{
				bool bBrow = Curve.Key.ToString().Contains(TEXT("Brow"));
				bool bHead = false;//  Curve.Key.ToString().Contains(TEXT("Head"));

				if (cnt >= SmoothIterations && !bBrow)
				{
					continue;
				}

				auto& Points = Curve.Value.Values;
				int32 Num = Points.Num();
				for (int32 i = 2; i < Num - 2; i++)
				{
					float NewVal = 0.f;
					if (bBrow || bHead)
					{
						NewVal = (Points[i - 1].Value + Points[i].Value + Points[i + 1].Value) / 3.f;
					}
					if (!bBrow)
					{
						NewVal = (Points[i - 2].Value + Points[i - 1].Value + Points[i].Value + Points[i + 1].Value + Points[i + 2].Value) * 0.2f;
					}
					Points[i].Value = FMath::Lerp(Points[i].Value, NewVal, FacialAnimationSmoothness);
				}
				if (Num > 1)
				{
					float NewVal = (Points[0].Value + Points[1].Value) * 0.5f;
					Points[0].Value = FMath::Lerp(Points[0].Value, NewVal, FacialAnimationSmoothness);
					if (Num > 2)
					{
						NewVal = (Points[Num - 2].Value + Points[Num - 1].Value) * 0.5f;
						Points[Num - 1].Value = FMath::Lerp(Points[Num - 1].Value, NewVal, FacialAnimationSmoothness);
					}
				}
			}
		} // end for
	} // end apply smoothness

	// convert to skeleton curves
	if (bFacialAnimationToSkeletonCurves && IsValid(MetaFaceSettings.ArKitCurvesPoseAsset))
	{
		ConvertFacialAnimCurves(OutAnimationCurves, MetaFaceSettings.ArKitCurvesPoseAsset);
	}
}

void UMFFunctionLibrary::ConvertFacialAnimCurves(TMap<FName, FSimpleFloatCurve>& InOutAnimationCurves, UPoseAsset* CurvesPoseAsset, FString Filter)
{
	const FName Head_Roll = TEXT("HeadRoll");
	const FName Head_Pitch = TEXT("HeadPitch");
	const FName Head_Yaw = TEXT("HeadYaw");

	TMap<FName, FSimpleFloatCurve> HeadData;
	HeadData.Add(Head_Roll);
	HeadData.Add(Head_Pitch);
	HeadData.Add(Head_Yaw);
	InOutAnimationCurves.RemoveAndCopyValue(Head_Roll, HeadData[Head_Roll]);
	InOutAnimationCurves.RemoveAndCopyValue(Head_Pitch, HeadData[Head_Pitch]);
	InOutAnimationCurves.RemoveAndCopyValue(Head_Yaw, HeadData[Head_Yaw]);

	TMap<FName, FSimpleFloatCurve> SwapData;
	UYnnkLipSyncFunctionLibrary::ExpandPoseAnimationToCurves(InOutAnimationCurves, CurvesPoseAsset, SwapData, Filter);
	InOutAnimationCurves = SwapData;

	InOutAnimationCurves.Append(HeadData);
}

void UMFFunctionLibrary::GetMetaFaceCurvesSet(TArray<FName>& CurvesSet, bool bLipSyncCurves)
{
	if (bLipSyncCurves)
	{
		CurvesSet =
		{
			TEXT("JawOpen"), TEXT("MouthClose"), TEXT("MouthFunnel"), TEXT("MouthPucker"), TEXT("MouthLeft"), TEXT("MouthRight"),
			TEXT("MouthSmileLeft"), TEXT("MouthSmileRight"), TEXT("MouthFrownLeft"), TEXT("MouthFrownRight"), TEXT("MouthDimpleLeft"),
			TEXT("MouthDimpleRight"), TEXT("MouthStretchLeft"), TEXT("MouthStretchRight"), TEXT("MouthRollLower"), TEXT("MouthRollUpper"),
			TEXT("MouthShrugLower"), TEXT("MouthShrugUpper"), TEXT("MouthPressLeft"), TEXT("MouthPressRight"), TEXT("MouthLowerDownLeft"),
			TEXT("MouthLowerDownRight"), TEXT("MouthUpperUpLeft"), TEXT("MouthUpperUpRight")
		}; // 24
	}
	else /* facial animation curves set */
	{
		CurvesSet =
		{
			TEXT("BrowDownLeft"), TEXT("BrowDownRight"), TEXT("BrowInnerUp"), TEXT("BrowOuterUpLeft"), TEXT("BrowOuterUpRight"),
			TEXT("CheekPuff"), TEXT("CheekSquintLeft"), TEXT("CheekSquintRight"), TEXT("NoseSneerLeft"), TEXT("NoseSneerRight"),
			TEXT("HeadYaw"), TEXT("HeadPitch"), TEXT("HeadRoll"), TEXT("EyeBlinkLeft"), TEXT("EyeSquintLeft"), TEXT("EyeWideLeft"),
			TEXT("EyeSquintRight"), TEXT("EyeWideRight"), TEXT("MouthSmileLeft"), TEXT("MouthSmileRight")
		}; // 20
	}
}

#undef __is_anim_converted
#undef __set_anim_converted