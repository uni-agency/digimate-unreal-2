// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "YnnkTypes.h"
#include "HAL/CriticalSection.h"
#include "MetaFaceTypes.h"
#include "MetaFaceFunctionLibrary.h"
#include "Runtime/Launch/Resources/Version.h"
#include "AsyncAnimBuilder.generated.h"

class UYnnkVoiceLipsyncData;
class UNeuralProcessWrapper;

DECLARE_DELEGATE_ThreeParams(FAsyncMetaFaceQueueResult, UYnnkVoiceLipsyncData*, int32, int32);

/**
 * Helper class to build facial animation from phonemes data
 */
UCLASS(BlueprintType)
class YNNKMETAFACEENHANCER_API UAsyncAnimBuilder : public UObject
{
	GENERATED_BODY()

public:
	UAsyncAnimBuilder();

	// Settings (imported from UYnnkMetaFaceSettings or UYnnkMetaFaceController)
	UPROPERTY()
	float EmotionsIntensity;
	UPROPERTY()
	float VisemeApplyAlpha;
	UPROPERTY()
	float LipsyncNeuralIntensity;
	UPROPERTY()
	float LipsyncSmoothness;
	UPROPERTY()
	float FacialAnimationSmoothness;
	UPROPERTY()
	class UPoseAsset* ArKitCurvesPoseAsset;
	UPROPERTY()
	bool bLipSyncToSkeletonCurves;
	UPROPERTY()
	bool bFacialAnimationToSkeletonCurves;
	UPROPERTY()
	bool bBalanceSmileFrownCurves;

#if ENGINE_MAJOR_VERSION > 4
	TObjectPtr<UNeuralProcessWrapper> NeuralProcessor;
#else
	UNeuralProcessWrapper* NeuralProcessor;
#endif

	// Note: Updated from EAsyncExecution::Thread
	UPROPERTY()
	TMap<FName, FSimpleFloatCurve> OutLipsyncData;

	// Note: Updated from EAsyncExecution::Thread
	UPROPERTY()
	TMap<FName, FSimpleFloatCurve> OutFacialAnimationData;

	// Create new object
	static UAsyncAnimBuilder* CreateAsyncAnimBuilder(UYnnkVoiceLipsyncData* InLipsyncData, bool bLipsync, bool bFacialAnimation, const FAsyncFacialAnimationResult& InCallbackEvent);

	static UAsyncAnimBuilder* CreateAsyncAnimBuilder(UYnnkVoiceLipsyncData* InLipsyncData, bool bLipsync, bool bFacialAnimation);

	// Initialize to update multiple assets
	void StartAsQueue(TArray<UYnnkVoiceLipsyncData*>& InOutLipsyncDataAssets, const FAsyncMetaFaceQueueResult& InProcessResultEvent);

	// Use settings from UYnnkMetaFaceController instead of default project settings
	void OverrideSettings(class UYnnkMetaFaceController* Controller);

	// Create async thread to build animation
	void Start();

protected:
	UPROPERTY()
	UYnnkVoiceLipsyncData* LipsyncData;

	UPROPERTY()
	bool bSaveGeneratedAnimationInLipsyncData;

	UPROPERTY()
	TArray<UYnnkVoiceLipsyncData*> ProcessingQueue;

	FAsyncMetaFaceQueueResult ProcessResultEvent;

	UPROPERTY()
	int32 QueueSize;

	UPROPERTY()
	bool bGenerateLipsync;

	UPROPERTY()
	bool bGenerateFacialAnimation;

	// Event to return analysis results
	UPROPERTY()
	FAsyncFacialAnimationResult CallbackEvent;

	/** Is active? */
	UPROPERTY()
	bool bIsWorking;
	UPROPERTY()
	bool bExecutionInterrupted;

	TFuture<void> WorkingThreadFA;
	UPROPERTY()
	bool bLipsyncReady;
	UPROPERTY()
	bool bFacialAnimationReady;

	UFUNCTION()
	void OnAnimationReady();

	UFUNCTION()
	void OnAnimationFailed();

public:
	UPROPERTY()
	bool bInterruptLastRequest;

	UFUNCTION(BlueprintPure, Category = "Async Recognizer")
	bool IsWorking() const;

	UFUNCTION(BlueprintCallable, Category = "Async Recognizer")
	void Stop();

	static FCriticalSection StartAsyncMutex;

};
