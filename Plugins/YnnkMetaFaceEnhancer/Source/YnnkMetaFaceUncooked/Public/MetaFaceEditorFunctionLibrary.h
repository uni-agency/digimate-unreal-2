// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "YnnkTypes.h"
#include "MetaFaceTypes.h"
#include "MetaFaceEditorFunctionLibrary.generated.h"

class UYnnkVoiceLipsyncData;
class UAnimSequence;
class UPoseAsset;

/**
 * MetaFace (MetaHuman Face Animation functions library)
 */
UCLASS()
class YNNKMETAFACEUNCOOKED_API UMetaFaceEditorFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Ynnk MetaFace Editor")
	static bool BakeFacialAnimation(
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
		bool bSaveArKitCurves,
		FString Filter = TEXT(""));

	static void NormalizeAnimation(TMap<FName, FSimpleFloatCurve>& InOutAnimation);
	static void NormalizeCurve(FSimpleFloatCurve& InOutAnimation);

	static void AddFloatKeysToCurve(UAnimSequence* AnimationSequence, const FName& CurveName, const FSimpleFloatCurve& CurveKeys, float UseTimeOffset, float UseSampleRate, bool bAdditive = false, bool bShouldTransact = true);

	static void GetListOfARFacialCurves(TArray<FName>& CurvesSet);

protected:
	static float FindMaxValueInCurve(const FSimpleFloatCurve& Curve);
};
