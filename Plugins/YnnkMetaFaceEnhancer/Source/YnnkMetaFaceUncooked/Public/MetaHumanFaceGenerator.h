// (c) Yuri N. K. 2022. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "AnimationModifier.h"
#include "Animation/PoseAsset.h"
#include "YnnkLipsyncController.h"
#include "YnnkTypes.h"
#include "MetaHumanFaceGenerator.generated.h"

/**
 * Animation modifier to create lip-sync curves from UYnnkVoiceLipsyncData asset
 */
UCLASS()
class YNNKMETAFACEUNCOOKED_API UMetaHumanFaceGenerator : public UAnimationModifier
{
	GENERATED_BODY()
	
public:
	UMetaHumanFaceGenerator();

	/* Lipsync curves source (Raw Curves should be generated!) */
	UPROPERTY(EditAnywhere, Category = "Setup")
	class UYnnkVoiceLipsyncData* Source;

	/** Pose Asset to convert ArKit curves to raw curves (i. e. morph targets or Rig controllers) */
	UPROPERTY(EditAnywhere, Category = "Setup")
	UPoseAsset* ArKitCurvesPoseAsset;

	/** Offset to add to timeline */
	UPROPERTY(EditAnywhere, Category = "Setup")
	float TimeOffset;

	/** Should add animation notify for used voice sound asset? */
	UPROPERTY(EditAnywhere, Category = "Setup")
	bool bCreateSoundNotify;

	/** Frequency to bake animation (frames per second) */
	UPROPERTY(EditAnywhere, Category = "Setup")
	int32 SampleRate = 30;

	/** Add animation to existing curves */
	UPROPERTY(EditAnywhere, Category = "Setup")
	bool bAdditive = false;

	/** Filter to process curves */
	UPROPERTY(EditAnywhere, Category = "Setup")
	FString InFilter;

	/** Filter to final curvers set */
	UPROPERTY(EditAnywhere, Category = "Setup")
	TArray<FName> OutFilter;

	/** Should save MetaFace AR curves in baked animation? */
	UPROPERTY(EditAnywhere, meta=(DisplayName="Save AR Curves"), Category = "Setup")
	bool bSaveArKitCurves = false;

	/** Should normalize curves (Ynnk Lip-Sync, MF lip-sync, MF facial animation) before baking animation? */
	UPROPERTY(EditAnywhere, Category = "Curves")
	bool bNormalizeInputCurves = true;

	/** Should normalize curves of baked animation? */
	UPROPERTY(EditAnywhere, Category = "Curves")
	bool bNormalizeFinalCurves = false;

	/** Linear interpolation between MetaFace lip-sync and Ynnk Lip-Sync (1 = 100% MetaFace) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"), Category = "Curves")
	float MetaToYnnkBlendAlpha = 0.72f;

	/** General power of lip-sync animation */
	UPROPERTY(EditAnywhere, Category = "Curves")
	float LipsyncPower = 1.f;

	/** Power of facial animation (like brows etc) */
	UPROPERTY(EditAnywhere, Category = "Curves")
	float FacialAnimationPower = 1.f;

	/* UAnimationModifier overrides */
	virtual void OnApply_Implementation(UAnimSequence* AnimationSequence) override;
	virtual void OnRevert_Implementation(UAnimSequence* AnimationSequence) override;
	/* UAnimationModifier overrides end */
};
