// (c) Yuri N. K. 2022. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "MetaFaceTypes.h"
#include "Templates/SubclassOf.h"
#include "ArKitPoseAssetGenerator.generated.h"

class USkeletalMesh;
class UAnimSequence;
class ULiveLinkRemapAsset;

/**
 * Helper object to generate default pose assets from ArKit curves to pose asset containing low-level skeletal mesh animation.
 * To generate pose asset, we need ULiveLinkRemapAsset (used for CC3 and provided in the default retarget-to-Unreal project).
 */
UCLASS()
class YNNKMETAFACEUNCOOKED_API UArKitPoseAssetGenerator : public UObject
{
	GENERATED_BODY()
	
public:
	UArKitPoseAssetGenerator();

	// Skeletal Mesh to use with lip-sync (face/head or full-body)
	UPROPERTY(EditAnywhere, Category = "Global")
	USkeletalMesh* TargetSkeletalMesh;

	// Select LiveLink RemapAsset (for CC3/CC4)
	UPROPERTY(EditAnywhere, Category = "Generation")
	TSubclassOf<ULiveLinkRemapAsset> CurvesRemapAsset;

	// Key is name of ArKit curve, value is corresponding name of internal curve (Morph Target or controller of applied Control Rig)
	UPROPERTY(EditAnywhere, Category = "Generation")
	TMap<FName, FName> ManualCurvesBinding;

	// Key is ArKit curve name, value is corresponding bone rotation
	UPROPERTY(EditAnywhere, Category = "Generation")
	TMap<FName, FMetaFaceCurveToBone> CurveToBoneBinding;

	// Create two different pose assets for curves (morph targets) and bones animation
	// This makes sense, because bones animation should be additive
	UPROPERTY(EditAnywhere, meta=(EditCondition="!CurveToBoneBinding.IsEmpty()"), Category = "Generation")
	bool bSepareteBonesAndBlends = true;

	// Use all settings to generate pose asset
	bool GeneratePoseAsset();

protected:
	// Initial animation data (for ArKit curves)
	UPROPERTY()
	TMap<FName, FSimpleFloatCurve> SourceAnimationData;
	// Remapped curves
	UPROPERTY()
	TMap<FName, FSimpleFloatCurve> AnimationData;

	UPROPERTY()
	TArray<FName> ArKitCurveNames;

	// Source AnimationData, CurvesRemapAsset. Target: AnimationData
	void RemapCurves();

	// Source AnimationData, TargetSkeletalMesh
	TObjectPtr<UAnimSequence> GenerateAnimSequenceFromData(FString Postfix) const;
	// Source: AnimationData, JawBoneName, JawBoneRotationTarget
	void GenerateBonesAnimation(TObjectPtr<UAnimSequence> AnimSequence) const;

	FName GetRemappedCurveName(ULiveLinkRemapAsset* RemapAsset, const FName& InCurveName) const;
};
