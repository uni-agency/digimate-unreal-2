// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_HeadIK.h"
#include "Animation/AnimInstance.h"
#include "UObject/AnimPhysObjectVersion.h"
#include "Components/SkeletalMeshComponent.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_HeadIK

#define LOCTEXT_NAMESPACE "AnimGraph_HeadIK"

UAnimGraphNode_HeadIK::UAnimGraphNode_HeadIK(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_HeadIK::GetControllerDescription() const
{
	return LOCTEXT("HeadIKNode", "Head IK");
}

FText UAnimGraphNode_HeadIK::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_HeadIK_Tooltip", "This node allow a head to trace or follow another bone or target");
}

FText UAnimGraphNode_HeadIK::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if ((TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle) && (Node.BoneToModify.BoneName == NAME_None))
	{
		return GetControllerDescription();
	}
	// @TODO: the bone can be altered in the property editor, so we have to 
	//        choose to mark this dirty when that happens for this to properly work
	else //if (!CachedNodeTitles.IsTitleCached(TitleType, this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ControllerDescription"), GetControllerDescription());
		Args.Add(TEXT("BoneName"), FText::FromName(Node.BoneToModify.BoneName));

		// FText::Format() is slow, so we cache this to save on performance
		if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_HeadIK_ListTitle", "{ControllerDescription} - Bone: {BoneName}"), Args), this);
		}
		else
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_HeadIK_Title", "{ControllerDescription}\nBone: {BoneName}"), Args), this);
		}
	}
	return CachedNodeTitles[TitleType];		
}

void UAnimGraphNode_HeadIK::Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelMeshComp) const
{
	if(SkelMeshComp)
	{
		if(FAnimNode_HeadIK* ActiveNode = GetActiveInstanceNode<FAnimNode_HeadIK>(SkelMeshComp->GetAnimInstance()))
		{
			ActiveNode->ConditionalDebugDraw(PDI, SkelMeshComp);
		}
	}
}


void UAnimGraphNode_HeadIK::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FAnimPhysObjectVersion::GUID);

	auto GetAlignVector = [](EAxisOption::Type AxisOption, const FVector& CustomAxis) -> FVector
	{
		switch (AxisOption)
		{
		case EAxisOption::X:
			return FTransform::Identity.GetUnitAxis(EAxis::X);
		case EAxisOption::X_Neg:
			return -FTransform::Identity.GetUnitAxis(EAxis::X);
		case EAxisOption::Y:
			return FTransform::Identity.GetUnitAxis(EAxis::Y);
		case EAxisOption::Y_Neg:
			return -FTransform::Identity.GetUnitAxis(EAxis::Y);
		case EAxisOption::Z:
			return FTransform::Identity.GetUnitAxis(EAxis::Z);
		case EAxisOption::Z_Neg:
			return -FTransform::Identity.GetUnitAxis(EAxis::Z);
		case EAxisOption::Custom:
			return CustomAxis;
		}

		return FVector(1.f, 0.f, 0.f);
	};
}

void UAnimGraphNode_HeadIK::GetOnScreenDebugInfo(TArray<FText>& DebugInfo, FAnimNode_Base* RuntimeAnimNode, USkeletalMeshComponent* PreviewSkelMeshComp) const
{
	if (RuntimeAnimNode)
	{
		const FAnimNode_HeadIK* HeadIKRuntimeNode = static_cast<FAnimNode_HeadIK*>(RuntimeAnimNode);
		DebugInfo.Add(FText::Format(LOCTEXT("DebugOnScreenBoneName", "Anim Look At (Source:{0})"), FText::FromName(HeadIKRuntimeNode->BoneToModify.BoneName)));

		if (HeadIKRuntimeNode->HeadIKTarget.HasValidSetup())
		{
			DebugInfo.Add(FText::Format(LOCTEXT("DebugOnScreenHeadIKTarget", "	Look At Target (Target:{0})"), FText::FromName(HeadIKRuntimeNode->HeadIKTarget.GetTargetSetup())));
		}
		else
		{
			DebugInfo.Add(FText::Format(LOCTEXT("DebugOnScreenHeadIKLocation", "	HeadIKLocation: {0}"), FText::FromString(HeadIKRuntimeNode->HeadIKLocation.ToString())));
		}

		DebugInfo.Add(FText::Format(LOCTEXT("DebugOnScreenTargetLocation", "	TargetLocation: {0}"), FText::FromString(HeadIKRuntimeNode->GetCachedTargetLocation().ToString())));
	}
}

#undef LOCTEXT_NAMESPACE
