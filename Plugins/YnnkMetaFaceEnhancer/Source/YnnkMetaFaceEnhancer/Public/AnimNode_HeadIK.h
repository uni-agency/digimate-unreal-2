// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BoneIndices.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "CommonAnimTypes.h"
#include "AnimNode_HeadIK.generated.h"

class FPrimitiveDrawInterface;
class USkeletalMeshComponent;

/**
 *	Simple controller that make a bone to look at the point or another bone
 */
USTRUCT(BlueprintInternalUseOnly)
struct YNNKMETAFACEENHANCER_API FAnimNode_HeadIK : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

	/** Name of bone to control. This is the main bone chain to modify from. **/
	UPROPERTY(EditAnywhere, Category=SkeletalControl) 
	FBoneReference BoneToModify;

	/** Target socket to look at. Used if HeadIKBone is empty. - You can use  HeadIKLocation if you need offset from this point. That location will be used in their local space. **/
	UPROPERTY(EditAnywhere, Category = Target)
	FBoneSocketTarget HeadIKTarget;

	/** Target Offset. It's in world space if HeadIKBone is empty or it is based on HeadIKBone or HeadIKSocket in their local space*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Target, meta = (PinHiddenByDefault))
	FVector HeadIKLocation;

	UPROPERTY(EditAnywhere, Category = SkeletalControl)
	FAxis HeadIK_Axis;

	/** Whether or not to use Look up axis */
	UPROPERTY(EditAnywhere, Category = SkeletalControl)
	bool bUseLookUpAxis;

	UPROPERTY(EditAnywhere, Category = SkeletalControl)
	FAxis LookUp_Axis;

	/** neck2, then neck1... */
	UPROPERTY(EditAnywhere, Category = SkeletalControl)
	TArray<float> NeckBonesAlpha;

	UPROPERTY()
	float ApplyAlpha;

	TArray<FCompactPoseBoneIndex> NeckBoneIndices;

	/** Look at Clamp value in degrees - if your look at axis is Z, only X, Y degree of clamp will be used */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SkeletalControl, meta=(PinHiddenByDefault))
	float HeadIKClamp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SkeletalControl, meta=(PinHiddenByDefault))
	float InterpolationTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SkeletalControl, meta=(PinHiddenByDefault))
	float InterpolationTriggerThreashold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SkeletalControl, meta = (PinHiddenByDefault))
	bool bEnableIK;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SkeletalControl, meta = (PinHiddenByDefault))
	float InterpolationSpeedIn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SkeletalControl, meta = (PinHiddenByDefault))
	float InterpolationSpeedOut;

	// in the future, it would be nice to have more options, -i.e. lag, interpolation speed
	FAnimNode_HeadIK();

	// FAnimNode_Base interface
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateComponentSpaceInternal(FComponentSpacePoseContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	FVector GetCachedTargetLocation() const {	return 	CachedCurrentTargetLocation;	}

#if WITH_EDITOR
	void ConditionalDebugDraw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* MeshComp) const;
#endif // WITH_EDITOR

private:
	// FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	/** Turn a linear interpolated alpha into the corresponding AlphaBlendType */
	static float AlphaToBlendType(float InAlpha);

	/** Debug transient data */
	FVector CurrentHeadIKLocation;

	/** Current Target Location */
	FVector CurrentTargetLocation;
	FVector PreviousTargetLocation;

	/** Current Alpha */
	float AccumulatedInterpoolationTime;


#if !UE_BUILD_SHIPPING
	/** Debug draw cached data */
	FTransform CachedOriginalTransform;
	FTransform CachedHeadIKTransform;
	FTransform CachedTargetCoordinate;
	FVector CachedPreviousTargetLocation;
	FVector CachedCurrentHeadIKLocation;
#endif // UE_BUILD_SHIPPING
	FVector CachedCurrentTargetLocation;
};
