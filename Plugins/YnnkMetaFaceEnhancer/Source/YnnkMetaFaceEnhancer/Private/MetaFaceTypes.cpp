// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

#include "MetaFaceTypes.h"
#include "YnnkTypes.h"
#include "Engine/World.h"
#include "YnnkLipsyncController.h"
#include "YnnkMetaFaceController.h"
#include "Animation/PoseAsset.h"
#include "AsyncAnimBuilder.h"

/* --------------------------------------------------------------- */
/* -					FMHFacialAnimation						 - */
/* --------------------------------------------------------------- */

void FMHFacialAnimation::Initialize(const TMap<FName, FSimpleFloatCurve>& InAnimationData, bool bInFadeOnPause, float InFadePauseDuration, float InFadeTime)
{
	AnimationData = InAnimationData;
	bFadeOnPause = bInFadeOnPause;
	Fade_PauseDuration = InFadePauseDuration;
	FadeTime = InFadeTime;
	bPlaying = bInterrupting = false;

	AnimationFrame.Empty();
	AnimationDuration = 0.f;
	for (const auto& Pair : AnimationData)
	{
		AnimationDuration = FMath::Max(AnimationDuration, Pair.Value.GetDuration());
		AnimationFrame.Add(Pair.Key, 0.f);
	}
}

void FMHFacialAnimation::ProcessFrame(float PlayTime, UYnnkLipsyncController* LipsyncController)
{
	if (bPlaying)
	{
		float Alpha = Intensity;
		// Global fade-in
		if (PlayTime < 0.3f)
		{
			Alpha *= (PlayTime / 0.3f);
		}

		if (PlayTime > AnimationDuration)
		{
			bPlaying = false;
			bInterrupting = true;
		}

		// get current viseme values
		for (auto& Curve : AnimationData)
		{
			const FName CurveName = Curve.Key;
			float PauseAlpha = 1.f;

			float t0, t1;
			if (bFadeOnPause && LipsyncController)
			{
				LipsyncController->GetSpeakingKeyIntervals(t0, t1);
			}
			else
			{
				Curve.Value.GetIntervalsToKeys(PlayTime, t0, t1);
			}

			if (t1 + t0 > Fade_PauseDuration && CurveName.ToString().Left(4) != TEXT("Head"))
			{
				if (t0 < FadeTime)
				{
					PauseAlpha = 1.f - t0 / FadeTime;
				}
				else if (t1 < FadeTime)
				{
					PauseAlpha = 1.f - t1 / FadeTime;
				}
				else
				{
					PauseAlpha = 0.f;
				}
			}

			AnimationFrame[CurveName] = Curve.Value.GetValueAtTime(PlayTime) * Alpha * PauseAlpha;
		}
	}
	else if (bInterrupting)
	{
		float dValue = LipsyncController->GetWorld()->GetDeltaSeconds() * 2.f;
		bool bCanFinish = true;

		for (auto& Curve : AnimationFrame)
		{
			if (Curve.Value > 0.f)
			{
				Curve.Value -= dValue;

				if (Curve.Value < 0.f)
					Curve.Value = 0.f;

				if (Curve.Value > 0.f)
					bCanFinish = false;
			}
		}

		if (bCanFinish)
		{
			bInterrupting = false;
		}
	}
}

void FMHFacialAnimation::Play()
{
	bPlaying = true;
	bInterrupting = false;
}

void FMHFacialAnimation::Stop()
{
	if (bPlaying)
	{
		bPlaying = false;
		bInterrupting = true;
	}
}

FString FMHFacialAnimation::GetDescription() const
{
	FString ret = TEXT("IsValid: ") + FString::FromInt((int)IsValid()) + TEXT("\n");
	for (auto& Curve : AnimationData)
	{
		float TimeFrom = -1.f, TimeTo = -1.f;
		if (Curve.Value.Values.Num() > 0)
		{
			TimeFrom = Curve.Value.Values[0].Time;
			TimeTo = Curve.Value.Values.Last().Time;
		}

		float Val = 0.f;
		for (const auto& v : Curve.Value.Values)
		{
			Val += v.Value;
		}
		Val /= Curve.Value.Values.Num();

		FString crv = Curve.Key.ToString() + TEXT(": [time ") + FString::SanitizeFloat(TimeFrom) + TEXT("/") + FString::SanitizeFloat(TimeTo) + TEXT("] average: ") + FString::SanitizeFloat(Val);

		ret += (crv + TEXT("\n"));
	}

	return ret;
}

FMetaFaceGenerationSettings::FMetaFaceGenerationSettings(UYnnkMetaFaceController* Controller)
{
	VisemeApplyAlpha = Controller->VisemeApplyAlpha;
	LipsyncNeuralIntensity = Controller->LipsyncNeuralIntensity;
	LipsyncSmoothness = Controller->LipsyncSmoothness;
	ArKitCurvesPoseAsset = Controller->ArKitCurvesPoseAsset;
	bLipSyncToSkeletonCurves = Controller->bLipSyncToSkeletonCurves;
	bFacialAnimationToSkeletonCurves = Controller->bFacialAnimationToSkeletonCurves;
	FacialAnimationSmoothness = Controller->FacialAnimationSmoothness;
}

FMetaFaceGenerationSettings::FMetaFaceGenerationSettings(UAsyncAnimBuilder* AsyncBuilder)
{
	VisemeApplyAlpha = AsyncBuilder->VisemeApplyAlpha;
	LipsyncNeuralIntensity = AsyncBuilder->LipsyncNeuralIntensity;
	LipsyncSmoothness = AsyncBuilder->LipsyncSmoothness;
	ArKitCurvesPoseAsset = AsyncBuilder->ArKitCurvesPoseAsset;
	bLipSyncToSkeletonCurves = bFacialAnimationToSkeletonCurves = false;
	FacialAnimationSmoothness = AsyncBuilder->FacialAnimationSmoothness;
}
