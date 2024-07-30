// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MetaFaceTypes.h"
#include "MetaFaceFunctionLibrary.generated.h"

DECLARE_DYNAMIC_DELEGATE_TwoParams(FAsyncFacialAnimationResult, const FMHFacialAnimation&, LipsyncAnimation, const FMHFacialAnimation&, FacialAnimation);

class UYnnkLipsyncController;
class UYnnkVoiceLipsyncData;
class UAsyncAnimBuilder;

/**
 * MetaFace (MetaHuman Face Animation functions library)
 */
UCLASS()
class YNNKMETAFACEENHANCER_API UMFFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	/**
	* Helper function for debugging
	*/
	UFUNCTION(BlueprintCallable, Category = "Ynnk MetaFace")
	static FString CopyFacialFrameToClipboard(const TMap<FName, float>& InData, const TArray<FString>& FilterCurves);

	/**
	* Fill two curves with lip-sync and facial animation in YnnkVoiceLipsyncData asset
	*/
	UFUNCTION(BlueprintCallable, Category = "Ynnk MetaFace")
	static void CreateMetaFaceAnimationCurves(UPARAM(Ref) UYnnkVoiceLipsyncData* LipsyncData, bool bCreateLipSync, bool bCreateFacialAnimation, const FMetaFaceGenerationSettings& MetaFaceSettings);

	/**
	* Initialize FacialAnimation object
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Initialize"), Category = "Ynnk MetaFace")
	static void FacialAnimation_Initialize(UPARAM(Ref) FMHFacialAnimation& Animation,
		const TMap<FName, FSimpleFloatCurve>& InAnimationData,
		float Intensity = 1.f,
		bool bInFadeOnPause = true,
		float InFadePauseDuration = 0.3f,
		float InFadeTime = 0.12f);

	/** Play Facial Animation */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Play"), Category = "Ynnk MetaFace")
	static void FacialAnimation_Play(UPARAM(Ref) FMHFacialAnimation& Animation);

	/** Stop Facial Animation */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Stop"), Category = "Ynnk MetaFace")
	static void FacialAnimation_Stop(UPARAM(Ref) FMHFacialAnimation& Animation);

	/** Tick Facial Animation */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Tick"), Category = "Ynnk MetaFace")
	static void FacialAnimation_Tick(UPARAM(Ref) FMHFacialAnimation& Animation, float PlayTime, UYnnkLipsyncController* LipsyncController);

	/** Evaluate Facial Animation curve at time */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Curve Value"), Category = "Ynnk MetaFace")
	static float FacialAnimation_CurveValueAtTime(UPARAM(Ref) FMHFacialAnimation& Animation, FName Curve, float PlayTime);

	/** Get description of Facial Animation object (for debugging) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Animation Description"), Category = "Ynnk MetaFace")
	static FString FacialAnimation_GetDescription(UPARAM(Ref) FMHFacialAnimation& Animation);

	/** Build lip-sync and facial animation objects for UYnnkVoiceLipsyncData object */
	UFUNCTION(BlueprintCallable, Category = "Ynnk MetaFace")
	static bool BuildFacialAnimationData(UPARAM(Ref) UYnnkVoiceLipsyncData* LipsyncData, bool bCreateLipSync, bool bCreateFacialAnimation, const FAsyncFacialAnimationResult& CallbackEvent);

	/** Enable YnnkMetaFace (call at startup) */
	UFUNCTION(BlueprintCallable, Category = "Ynnk MetaFace")
	static class UNeuralProcessWrapper* InitializeYnnkMetaFace(UObject* Parent);

	/** Initialize YnnkMetaFace with first invisible request. */
	UFUNCTION(BlueprintCallable, Category = "Ynnk MetaFace")
	static void PrepareYnnkMetaFaceModel();

	/** Generate head rotation from a specified frame of facial animation */
	UFUNCTION(BlueprintPure, Category = "Ynnk MetaFace")
	static FRotator MakeHeadRotatorFromAnimFrame(const TMap<FName, float>& AnimationFrame, float OffsetRoll, float OffsetPitch, float OffsetYaw);

	/** Expand curves to skeleton but preserve head rotation */
	static void ConvertFacialAnimCurves(TMap<FName, FSimpleFloatCurve>& InOutAnimationCurves, class UPoseAsset* CurvesPoseAsset, FString Filter = TEXT("CTRL_"));

	/** Convert raw animation data to animation curves */
	static void RawDataToLipsync(const UYnnkVoiceLipsyncData* PhonemesSource, const RawAnimDataMap& InData, TMap<FName, FSimpleFloatCurve>& OutAnimationCurves,
		const FMetaFaceGenerationSettings& MetaFaceSettings);
	/** Convert raw animation data to animation curves */
	static void RawDataToFacialAnimation(const UYnnkVoiceLipsyncData* PhonemesSource, const RawAnimDataMap& InData, TMap<FName, FSimpleFloatCurve>& OutAnimationCurves,
		const FMetaFaceGenerationSettings& MetaFaceSettings);

	static void GetMetaFaceCurvesSet(TArray<FName>& CurvesSet, bool bLipSyncCurves);

};
