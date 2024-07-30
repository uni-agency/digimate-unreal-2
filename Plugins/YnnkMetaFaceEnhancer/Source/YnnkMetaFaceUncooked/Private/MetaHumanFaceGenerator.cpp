// (c) Yuri N. K. 2022. All rights reserved.
// ykasczc@gmail.com


#include "MetaHumanFaceGenerator.h"
#include "MetaFaceEditorFunctionLibrary.h"
#include "LipsyncAnimationEditorLibrary.h"
#include "AnimationBlueprintLibrary.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "Animation/AnimSequence.h"
#include "Sound/SoundWave.h"
#include "YnnkVoiceLipsyncData.h"
#include "YnnkTypes.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealTypePrivate.h"

#define NOTIFY_TRACK_NAME TEXT("YnnkVoiceNotify")

UMetaHumanFaceGenerator::UMetaHumanFaceGenerator()
	: Source(nullptr)
{
	if (OutFilter.IsEmpty())
	{
		OutFilter = { TEXT("*") };
	}
	InFilter = TEXT("CTRL_*");
}

void UMetaHumanFaceGenerator::OnApply_Implementation(UAnimSequence* AnimationSequence)
{
	// Base UAnimationModifier class is wrapping OnApply_Implementation with Animation Controller's OpenBracket-CloseBracked

	if (!IsValid(Source) || !IsValid(ArKitCurvesPoseAsset))
	{
		return;
	}

	float MetaLipsyncPower = MetaToYnnkBlendAlpha * LipsyncPower;
	float YnnkLipsyncPower = (1.f - MetaToYnnkBlendAlpha) * LipsyncPower;
	
	// Bake three animations to single track
	TMap<FName, FSimpleFloatCurve> BakedData;
	if (!UMetaFaceEditorFunctionLibrary::BakeFacialAnimation(Source, BakedData, ArKitCurvesPoseAsset, bNormalizeInputCurves,
		FacialAnimationPower,
		MetaLipsyncPower,
		YnnkLipsyncPower,
		(float)SampleRate, EYnnkBlendType::YBT_Additive, true, bSaveArKitCurves, InFilter))
	{
		return;
	}

	// Normalize baked animation?
	if (bNormalizeFinalCurves)
	{
		for (auto& Curve : BakedData)
		{
			UMetaFaceEditorFunctionLibrary::NormalizeCurve(Curve.Value);
		}
	}
	
	// Create sound notification?
	if (bCreateSoundNotify && IsValid(Source->SoundAsset))
	{
		const FName NotifyTrackName = TEXT("YnnkMetaFace");
		if (UAnimationBlueprintLibrary::IsValidAnimNotifyTrackName(AnimationSequence, NotifyTrackName))
		{
			UAnimationBlueprintLibrary::RemoveAnimationNotifyEventsByTrack(AnimationSequence, NotifyTrackName);
		}
		else
		{
			UAnimationBlueprintLibrary::AddAnimationNotifyTrack(AnimationSequence, NotifyTrackName);			
		}
		UAnimNotify_PlaySound* SoundNotify = Cast<UAnimNotify_PlaySound>(UAnimationBlueprintLibrary::AddAnimationNotifyEvent(
			AnimationSequence,
			NotifyTrackName,
			TimeOffset,
			UAnimNotify_PlaySound::StaticClass()));

		if (SoundNotify)
		{
			SoundNotify->Sound = Source->SoundAsset;
		}
	}

	bool bNoFilter = (OutFilter.Num() == 0);
	for (const auto& FilterName : OutFilter)
	{
		if (FilterName.IsEqual(TEXT("*")))
		{
			bNoFilter = true; break;
		}
	}

	// Save baked curves to animation sequence
	for (const auto& Curve : BakedData)
	{
		// has filter
		if (!bNoFilter)
		{
			bool bFilterPassed = false;
			FString CurrCurveName = Curve.Key.ToString();
			for (const auto& FilterName : OutFilter)
			{
				FString FilterStr = FilterName.ToString();
				if (FilterStr == CurrCurveName)
				{
					bFilterPassed = true; continue;
				}
				else if (FilterStr.Right(1) == TEXT("*") && FilterStr.LeftChop(1) == CurrCurveName.Left(FilterStr.Len() - 1))
				{
					bFilterPassed = true; continue;
				}
				else if (FilterStr.Left(1) == TEXT("*") && FilterStr.RightChop(1) == CurrCurveName.Right(FilterStr.Len() - 1))
				{
					bFilterPassed = true; continue;
				}
			}
			if (!bFilterPassed)
			{
				continue;
			}
		}

		FAnimationCurveIdentifier CurveId(Curve.Key, ERawCurveTrackTypes::RCT_Float);
		if (!AnimationSequence->GetDataModel()->FindFloatCurve(CurveId))
		{
			AnimationSequence->GetController().AddCurve(CurveId, AACF_DefaultCurve, false);
		}

		UMetaFaceEditorFunctionLibrary::AddFloatKeysToCurve(AnimationSequence, Curve.Key, Curve.Value, TimeOffset, (float)SampleRate, bAdditive, false);
	}
}

void UMetaHumanFaceGenerator::OnRevert_Implementation(UAnimSequence* AnimationSequence)
{
}

