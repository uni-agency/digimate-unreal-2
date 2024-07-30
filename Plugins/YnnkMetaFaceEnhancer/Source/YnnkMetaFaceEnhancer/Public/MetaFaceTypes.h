// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "YnnkTypes.h"
#include "Runtime/Launch/Resources/Version.h"
#include "MetaFaceTypes.generated.h"

#ifndef _uev_backward_compatibility
	#define _uev_backward_compatibility

	#if ENGINE_MAJOR_VERSION > 4
		#define __uev_destory_object(obj) obj->MarkAsGarbage()
		#define __uev_access_skeleton(mesh) mesh->GetSkeleton()
		#define __uev_access_refskeleton(obj) obj->GetRefSkeleton()
	#else
		#define __uev_destory_object(obj) obj->MarkPendingKill()
		#define __uev_access_skeleton(mesh) mesh->Skeleton
		#define __uev_access_refskeleton(obj) obj->RefSkeleton

		#define FVector3f FVector
		#define FQuat4f FQuat
		#define FRotator3f FRotator
	#endif

	#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 1)
		#define FAppStyle FEditorStyle
		#define GetAppStyleSetName GetStyleSetName
	#endif
#endif

typedef TMap<FName, TArray<float>> RawAnimDataMap;

class UYnnkLipsyncController;

DECLARE_LOG_CATEGORY_EXTERN(LogMetaFace, Log, All);

/**
* MetaHuman eyes controller program
*/
UENUM(BlueprintType)
enum class EEyesControlType : uint8
{
	EC_Disabled				UMETA(DisplayName = "Disabled"),
	EC_LiveMovement			UMETA(DisplayName = "Live Movement"),
	EC_FocusAtTarget		UMETA(DisplayName = "Focus at Target"),

	EC_Max					UMETA(Hidden)
};

/**
* Object containing facial animation for MetaHuman
*/
USTRUCT(BlueprintType, meta = (DisplayName = "MH Facial Animation"))
struct YNNKMETAFACEENHANCER_API FMHFacialAnimation
{
	GENERATED_USTRUCT_BODY()

	// Is currently playing this animation?
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MH Facial Animation")
	bool bPlaying;

	// Is current play interrupted?
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MH Facial Animation")
	bool bInterrupting;

	// Curves Data
	UPROPERTY(BlueprintReadOnly, Category = "MH Facial Animation")
	TMap<FName, FSimpleFloatCurve> AnimationData;
	
	// Current frame of Animation Data. Apply it in animation blueprint.
	UPROPERTY(BlueprintReadOnly, Category = "MH Facial Animation")
	TMap<FName, float> AnimationFrame;

	// Animation intensity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MH Facial Animation")
	float Intensity;

	// Should fade animation on pause?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MH Facial Animation")
	bool bFadeOnPause;

	// Duration of pause between keys to trigger fade
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MH Facial Animation")
	float Fade_PauseDuration;

	// Fade in/out duration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MH Facial Animation")
	float FadeTime;

	// Length of animation data
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MH Facial Animation")
	float AnimationDuration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MH Facial Animation")
	uint8 AnimationFlag;

	FMHFacialAnimation()
		: bPlaying(false)
		, bInterrupting(false)
		, Intensity(1.f)
		, bFadeOnPause(true)
		, Fade_PauseDuration(0.3f)
		, FadeTime(0.12f)
		, AnimationDuration(0.f)
		, AnimationFlag(0)
	{};

	void Initialize(const TMap<FName, FSimpleFloatCurve>& InAnimationData, bool bInFadeOnPause, float InFadePauseDuration = 0.3f, float InFadeTime = 0.12f);
	void ProcessFrame(float PlayTime, UYnnkLipsyncController* LipsyncController);
	void Play();
	void Stop();
	bool IsValid() const { return AnimationData.Num() > 0 && AnimationFrame.Num() > 0; }
	bool IsActive() const { return bPlaying || bInterrupting; }
	FString GetDescription() const;

	FMHFacialAnimation& operator=(const FMHFacialAnimation& OtherItem)
	{
		this->Initialize(OtherItem.AnimationData, OtherItem.bFadeOnPause, OtherItem.Fade_PauseDuration, OtherItem.FadeTime);
		this->Intensity = OtherItem.Intensity;
		this->AnimationDuration = OtherItem.AnimationDuration;
		return *this;
	}
};

/** Animation curves preset */
USTRUCT(BlueprintType)
struct YNNKMETAFACEENHANCER_API FMetaFacePose
{
	GENERATED_USTRUCT_BODY()

	// Curve Name --> Curve Value map
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta Face Pose")
	TMap<FName, float> Curves;

	FMetaFacePose()
	{};

	FMetaFacePose(const TMap<FName, float>& InCurves)
		: Curves(InCurves)
	{};
};

/** Rule to convert curve animation to bone animation */
USTRUCT(BlueprintType)
struct YNNKMETAFACEENHANCER_API FMetaFaceCurveToBone
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta Face Curve to Bone")
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta Face Curve to Bone")
	FRotator Orientation;

	FMetaFaceCurveToBone() {};
	FMetaFaceCurveToBone(const FName& InName, const FRotator& InOrientation)
		: BoneName(InName), Orientation(InOrientation)
	{}
};

/** Generalized settings used to generate metaface animation */
USTRUCT(BlueprintType)
struct YNNKMETAFACEENHANCER_API FMetaFaceGenerationSettings
{
	GENERATED_USTRUCT_BODY()

	// Pose Asset with ArKit curves (needed if LipSyncToSkeletonCurves or FacialAnimationToSkeletonCurves is set)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta Face Generation Settings")
	class UPoseAsset* ArKitCurvesPoseAsset = nullptr;

	// Set if you use CC3 or CC4 model with CC3-Traditional+ facial preset
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="CC3-Traditional Asset"), Category = "Meta Face Generation Settings")
	bool bBalanceSmileFrownCurves = false;

	// Convert generated lip-sync animation to skeletal curves/morph targets (ArKitCurvesPoseAsset is requires)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta Face Generation Settings")
	bool bLipSyncToSkeletonCurves = false;

	// Convert generated facial animation to skeletal curves/morph targets (ArKitCurvesPoseAsset is requires)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta Face Generation Settings")
	bool bFacialAnimationToSkeletonCurves = false;

	// Should be always 1
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta Face Generation Settings")
	float LipsyncNeuralIntensity = 1.f;

	// Intensity of generated lip-sync animation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta Face Generation Settings")
	float VisemeApplyAlpha = 1.f;

	// Apply smoothness to animation (0..1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta Face Generation Settings")
	float LipsyncSmoothness = 0.3f;

	// Apply smoothness to animation (0..1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta Face Generation Settings")
	float FacialAnimationSmoothness = 1.f;

	FMetaFaceGenerationSettings() {};
	FMetaFaceGenerationSettings(class UYnnkMetaFaceController* Controller);
	FMetaFaceGenerationSettings(class UAsyncAnimBuilder* AsyncBuilder);
};