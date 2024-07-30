#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/SkeletalMesh.h"
#include "DMetaHumanOutfitDataAssetBase.generated.h"

UCLASS()
class DIGIMATE5_4_API UDMetaHumanOutfitDataAssetBase : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USkeletalMesh* HeadOutfit;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USkeletalMesh* TorsoOutfit;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USkeletalMesh* LegsOutfit;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USkeletalMesh* FeetOutfit;
};
