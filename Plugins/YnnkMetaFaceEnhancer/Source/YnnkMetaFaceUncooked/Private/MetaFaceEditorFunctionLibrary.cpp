// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

#include "MetaFaceEditorFunctionLibrary.h"
#include "Animation/AnimSequence.h"
#include "MetaFaceFunctionLibrary.h"
#include "AnimationBlueprintLibrary.h"
#include "YnnkLipSyncFunctionLibrary.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "YnnkTypes.h"
#include "MetaFaceTypes.h"
#include "YnnkVoiceLipsyncData.h"
#include "Animation/PoseAsset.h"
#include "YnnkMetaFaceEnhancer.h"
#include "YnnkMetaFaceController.h"
#include "Misc/MessageDialog.h"

#define Track_Facial ExtraAnimData2
#define Track_LipSync ExtraAnimData1

void UMetaFaceEditorFunctionLibrary::GetListOfARFacialCurves(TArray<FName>& CurvesSet)
{
	CurvesSet =
	{
		TEXT("EyeBlinkLeft"), TEXT("EyeLookDownLeft"), TEXT("EyeLookInLeft"), TEXT("EyeLookOutLeft"), TEXT("EyeLookUpLeft"), TEXT("EyeSquintLeft"), TEXT("EyeWideLeft"), TEXT("EyeBlinkRight"), TEXT("EyeLookDownRight"), TEXT("EyeLookInRight"),
		TEXT("EyeLookOutRight"), TEXT("EyeLookUpRight"), TEXT("EyeSquintRight"), TEXT("EyeWideRight"), TEXT("JawForward"), TEXT("JawRight"), TEXT("JawLeft"), TEXT("JawOpen"), TEXT("MouthClose"), TEXT("MouthFunnel"), TEXT("MouthPucker"),
		TEXT("MouthRight"), TEXT("MouthLeft"), TEXT("MouthSmileLeft"), TEXT("MouthSmileRight"), TEXT("MouthFrownLeft"), TEXT("MouthFrownRight"), TEXT("MouthDimpleLeft"), TEXT("MouthDimpleRight"), TEXT("MouthStretchLeft"), TEXT("MouthStretchRight"),
		TEXT("MouthRollLower"), TEXT("MouthRollUpper"), TEXT("MouthShrugLower"), TEXT("MouthShrugUpper"), TEXT("MouthPressLeft"), TEXT("MouthPressRight"), TEXT("MouthLowerDownLeft"), TEXT("MouthLowerDownRight"), TEXT("MouthUpperUpLeft"),
		TEXT("MouthUpperUpRight"), TEXT("BrowDownLeft"), TEXT("BrowDownRight"), TEXT("BrowInnerUp"), TEXT("BrowOuterUpLeft"), TEXT("BrowOuterUpRight"), TEXT("CheekPuff"), TEXT("CheekSquintLeft"), TEXT("CheekSquintRight"), TEXT("NoseSneerLeft"),
		TEXT("NoseSneerRight"), TEXT("TongueOut"), TEXT("HeadYaw"), TEXT("HeadPitch"), TEXT("HeadRoll"), TEXT("LeftEyeYaw"), TEXT("LeftEyePitch"), TEXT("LeftEyeRoll"), TEXT("RightEyeYaw"), TEXT("RightEyePitch"), TEXT("RightEyeRoll")
	};
}

bool UMetaFaceEditorFunctionLibrary::BakeFacialAnimation(
	const UYnnkVoiceLipsyncData* LipSyncData,
	TMap<FName, FSimpleFloatCurve>& BakedAnimationData,
	UPoseAsset* ArKitCurvesPoseAsset,
	bool bNormalizeInputCurves,
	float EmotionsAlpha,
	float MetaLipSyncAlpha,
	float YnnkLipSyncAlpha,
	float FrameRate,
	EYnnkBlendType YnnkLipSyncBlendType,
	bool bShowMessages,
	bool bSaveArKitCurves, FString Filter)
{
	/*
	lipsync CurrentLipsync.Initialize(PhraseAsset->ExtraAnimData1, false);
	auto AnimCopy = PhraseAsset->ExtraAnimData2;
	UMFFunctionLibrary::ConvertFacialAnimCurves(AnimCopy, ArKitCurvesPoseAsset);
	CurrentFaceAnim.Initialize(AnimCopy, true, 1.f, 0.49f);
	*/

	if (!IsValid(LipSyncData))
	{
		UE_LOG(LogTemp, Log, TEXT("Invalid YnnkVoiceLipsyncData asset"));
		return false;
	}

	if (LipSyncData->GetDuration() == 0.f || LipSyncData->RawCurves.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("YnnkVoiceLipsyncData asset (%s) is empty. Can't use it to bake animation."), *LipSyncData->GetName());
		if (bShowMessages)
		{
			FMessageDialog::Open(EAppMsgType::Type::Ok, FText::FromString("Provided source asset has zero duration. Make sure raw curves are generated (menu Asset -> Create Raw Curves...)"));
		}
		return false;
	}

	if (!Filter.IsEmpty() && Filter.Right(1) == TEXT("*"))
	{
		Filter.LeftChopInline(1);
	}

	// facial track is stored in ArKit curves
	TMap<FName, FSimpleFloatCurve> FacialTrack = LipSyncData->Track_Facial;
	UMFFunctionLibrary::ConvertFacialAnimCurves(FacialTrack, ArKitCurvesPoseAsset, Filter);
	// lip-dync track is stored in ArKit curves
	TMap<FName, FSimpleFloatCurve> LipSyncTrack = LipSyncData->Track_LipSync;
	UMFFunctionLibrary::ConvertFacialAnimCurves(LipSyncTrack, ArKitCurvesPoseAsset, Filter);

	if (EmotionsAlpha > 0.f && FacialTrack.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("YnnkVoiceLipsyncData asset (%s) doesn't contain facial animation. Aborting."), *LipSyncData->GetName());
		if (bShowMessages)
		{
			FMessageDialog::Open(EAppMsgType::Type::Ok, FText::FromString(TEXT("Provided source asset doesn't contain facial animation.")));
		}
		return false;
	}

	if (LipSyncTrack.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("YnnkVoiceLipsyncData asset (%s) doesn't contain MetaFace lip-sync animation. Aborting."), *LipSyncData->GetName());
		if (bShowMessages)
		{
			FMessageDialog::Open(EAppMsgType::Type::Ok, FText::FromString(TEXT("Provided source asset doesn't contain MetaFace lip-sync animation.")));
		}
		return false;
	}

	// Should normalize? Find multipliers
	float Mul_LS = 0.f, Mul_MFLS = 0.f, Mul_MFFA = 0.f;
	if (bNormalizeInputCurves)
	{
		Mul_LS = 0.f;
		for (const auto& Curve : LipSyncData->RawCurves)
		{
			Mul_LS = FMath::Max(Mul_LS, FMath::Abs(FindMaxValueInCurve(Curve.Value)));
		}
		Mul_LS = (Mul_LS == 0.f) ? 1.f : 1.f / Mul_LS;

		Mul_MFLS = 0.f;
		for (const auto& Curve : LipSyncTrack)
		{
			Mul_MFLS = FMath::Max(Mul_MFLS, FMath::Abs(FindMaxValueInCurve(Curve.Value)));
		}
		Mul_MFLS = (Mul_MFLS == 0.f) ? 1.f : 1.f / Mul_MFLS;

		Mul_MFFA = 0.f;
		for (const auto& Curve : FacialTrack)
		{
			Mul_MFFA = FMath::Max(Mul_MFFA, FMath::Abs(FindMaxValueInCurve(Curve.Value)));
		}
		Mul_MFFA = (Mul_MFFA == 0.f) ? 1.f : 1.f / Mul_MFFA;
	}

	// main loop
	const float FrameInterval = 1.f / (float)FrameRate;
	const float AnimationDuration = LipSyncData->GetDuration();
	float CurrentTime = 0.f;

	TSet<FName> CurvesList, Curves2, Curves3;

	LipSyncData->RawCurves.GetKeys(CurvesList);
	if (Filter != TEXT(""))
	{
		Curves2 = CurvesList;
		CurvesList.Empty();

		for (auto& Curve : Curves2)
		{
			if (Filter == Curve.ToString().Left(Filter.Len()))
			{
				CurvesList.Add(Curve);
			}
		}

		if (CurvesList.IsEmpty())
		{
			Filter = TEXT("");
			CurvesList = Curves2;
		}

		Curves2.Empty();
	}

	LipSyncTrack.GetKeys(Curves2);
	FacialTrack.GetKeys(Curves3);

	/*
	FString tmp = TEXT("");
	for (const auto& CurveName : CurvesList)
	{
		tmp.Append(TEXT("|") + CurveName.ToString());
	}
	UE_LOG(LogTemp, Log, TEXT("RawCurves = %s"), *tmp);
	
	tmp = TEXT("");
	for (const auto& CurveName : Curves2)
	{
		tmp.Append(TEXT("|") + CurveName.ToString());
	}
	UE_LOG(LogTemp, Log, TEXT("LipSyncTrack = %s"), *tmp);
	
	tmp = TEXT("");
	for (const auto& CurveName : Curves3)
	{
		tmp.Append(TEXT("|") + CurveName.ToString());
	}
	UE_LOG(LogTemp, Log, TEXT("FacialTrack = %s"), *tmp);
	*/

	CurvesList.Append(Curves2);
	CurvesList.Append(Curves3);
	
	// cleanup and prepare target map
	BakedAnimationData.Empty();
	for (const auto& CurveName : CurvesList)
	{
		BakedAnimationData.Add(CurveName);
	}

	TSet<FName> InvalidCurveNames;

	while (CurrentTime <= AnimationDuration)
	{
		for (const auto& CurveName : CurvesList)
		{
			bool bNoMetaFaceAnimation = false;
			if (!LipSyncTrack.Contains(CurveName) || !FacialTrack.Contains(CurveName))
			{
				if (!InvalidCurveNames.Contains(CurveName))
				{
					UE_LOG(LogTemp, Warning, TEXT("Facial animations (Ynnk Lip-Sync, Extra Track #1, Extra Track #2) use different sets of curves. Error generated by %s"), *CurveName.ToString());
					InvalidCurveNames.Add(CurveName);
				}
				bNoMetaFaceAnimation = true;
			}

			float LipSyncValue = LipSyncData->RawCurves.Contains(CurveName) ? LipSyncData->RawCurves[CurveName].GetValueAtTime(CurrentTime) : 0.f;
			float MFLipSyncValue = bNoMetaFaceAnimation ? 0.f : LipSyncTrack[CurveName].GetValueAtTime(CurrentTime);
			float MFFacial = bNoMetaFaceAnimation ? 0.f : FacialTrack[CurveName].GetValueAtTime(CurrentTime);

			if (bNormalizeInputCurves)
			{
				LipSyncValue *= Mul_LS;
				MFLipSyncValue *= Mul_MFLS;
				MFFacial *= Mul_MFFA;
			}

			float OutValue = MFLipSyncValue * MetaLipSyncAlpha;
			switch (YnnkLipSyncBlendType)
			{
				case EYnnkBlendType::YBT_Additive:	OutValue += LipSyncValue * YnnkLipSyncAlpha; break;
				case EYnnkBlendType::YBT_Clamp:		OutValue =  FMath::Min(OutValue, LipSyncValue * YnnkLipSyncAlpha); break;
				case EYnnkBlendType::YBT_Default:	OutValue =  FMath::Lerp(OutValue, LipSyncValue * YnnkLipSyncAlpha, 0.5f); break;
				case EYnnkBlendType::YBT_Multiply:	OutValue *= (LipSyncValue * YnnkLipSyncAlpha); break;
			}

			OutValue += (MFFacial * EmotionsAlpha);

			// save value
			BakedAnimationData[CurveName].Values.Add(FSimpleFloatValue(CurrentTime, OutValue));
		}

		// Increase time
		if (CurrentTime < AnimationDuration)
		{
			CurrentTime = FMath::Min(CurrentTime + FrameInterval, AnimationDuration);
		}
		else break;
	}

	if (bSaveArKitCurves)
	{
		TArray<FName> ArCurves;
		GetListOfARFacialCurves(ArCurves);

		for (const auto& CurveName : ArCurves)
		{
			if (!BakedAnimationData.Contains(CurveName))
			{
				if (LipSyncData->Track_LipSync.Contains(CurveName) && LipSyncData->Track_Facial.Contains(CurveName))
				{
					FSimpleFloatCurve CurveSumm = LipSyncData->Track_LipSync[CurveName];
					const auto& FacialCurvePoints = LipSyncData->Track_Facial[CurveName];
					for (auto& Point : CurveSumm.Values)
					{
						Point.Value = FMath::Clamp(Point.Value + FacialCurvePoints.GetValueAtTime(Point.Time), -1.f, 1.f);
					}

					BakedAnimationData.Add(CurveName, CurveSumm);
				}
				else if (LipSyncData->Track_LipSync.Contains(CurveName))
				{
					BakedAnimationData.Add(CurveName, LipSyncData->Track_LipSync[CurveName]);
				}
				else if (LipSyncData->Track_Facial.Contains(CurveName))
				{
					BakedAnimationData.Add(CurveName, LipSyncData->Track_Facial[CurveName]);
				}
			}
		}
	}

	return true;
}

float UMetaFaceEditorFunctionLibrary::FindMaxValueInCurve(const FSimpleFloatCurve& Curve)
{
	float Max = 0.f, Min = 0.f;
	for (const auto& Value : Curve.Values)
	{
		if (Value.Value > Max)
		{
			Max = Value.Value;
		}
		else if (Value.Value < Min)
		{
			Min = Value.Value;
		}
	}

	if (Max == 0.f && Min == 0.f)
	{
		return 1.f;
	}

	return (FMath::Abs(Max) > FMath::Abs(Min)) ? Max : Min;
}

void UMetaFaceEditorFunctionLibrary::NormalizeAnimation(TMap<FName, FSimpleFloatCurve>& InOutAnimation)
{
	float Multiplier = 0.f;
	for (const auto& Curve : InOutAnimation)
	{
		Multiplier = FMath::Max(Multiplier, FMath::Abs(FindMaxValueInCurve(Curve.Value)));
	}
	Multiplier = (Multiplier == 0.f) ? 1.f : 1.f / Multiplier;

	if (Multiplier > 1.f)
	{
		for (auto& Curve : InOutAnimation)
		{
			for (auto& Pair : Curve.Value.Values)
			{
				Pair.Value *= Multiplier;
			}
		}
	}
}

void UMetaFaceEditorFunctionLibrary::NormalizeCurve(FSimpleFloatCurve& InOutAnimation)
{
	float Multiplier = FMath::Abs(FindMaxValueInCurve(InOutAnimation));

	if (Multiplier > 1.f)
	{
		for (auto& Pair : InOutAnimation.Values)
		{
			Pair.Value *= Multiplier;
		}
	}
}

void UMetaFaceEditorFunctionLibrary::AddFloatKeysToCurve(UAnimSequence* AnimationSequence, const FName& CurveName, const FSimpleFloatCurve& CurveKeys, float UseTimeOffset, float UseSampleRate, bool bAdditive, bool bShouldTransact)
{
	ERawCurveTrackTypes t = UAnimationBlueprintLibrary::RetrieveCurveTypeForCurve(AnimationSequence, CurveName);
	if (t != ERawCurveTrackTypes::RCT_Float)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid type of curve (%s), expecting FLOAT"), *CurveName.ToString());
		return;
	}

	// Retrieve smart name for curve
	const FAnimationCurveIdentifier CurveId(CurveName, ERawCurveTrackTypes::RCT_Float);

	FRichCurve OutCurve;
	const FFloatCurve* InCurve = nullptr;

	bAdditive = bAdditive && CurveId.IsValid();
	if (bAdditive)
	{
		InCurve = static_cast<const FFloatCurve*>(AnimationSequence->GetDataModel()->FindCurve(CurveId));
	}

	if (UseTimeOffset > 0.f)
	{
		float StartTime = FMath::Max(0.f, UseTimeOffset - 1.f / UseSampleRate);
		float ApplyValue = bAdditive ? InCurve->FloatCurve.Eval(StartTime) : 0.f;

		FKeyHandle h = OutCurve.UpdateOrAddKey(StartTime, ApplyValue);
		OutCurve.SetKeyInterpMode(h, ERichCurveInterpMode::RCIM_Linear, false);
		OutCurve.SetKeyTangentMode(h, ERichCurveTangentMode::RCTM_Auto, true);
	}

	for (const auto& Pair : CurveKeys.Values)
	{
		const float UseTime = Pair.Time + UseTimeOffset;
		const float UseValue = Pair.Value + (bAdditive ? InCurve->FloatCurve.Eval(UseTime) : 0.f);

		FKeyHandle h = OutCurve.UpdateOrAddKey(UseTime, UseValue);
		OutCurve.SetKeyInterpMode(h, ERichCurveInterpMode::RCIM_Linear, false);
		OutCurve.SetKeyTangentMode(h, ERichCurveTangentMode::RCTM_Auto, true);
	}

	OutCurve.RemoveRedundantAutoTangentKeys(0.0001f);
	OutCurve.AutoSetTangents();

	// Save
	IAnimationDataController& Controller = AnimationSequence->GetController();
	Controller.SetCurveKeys(CurveId, OutCurve.Keys, bShouldTransact);
}

#undef Track_Facial
#undef Track_LipSync