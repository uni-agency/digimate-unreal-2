// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNode_HeadIK.h"
#include "Components/SkeletalMeshComponent.h"
#include "SceneManagement.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimationCoreLibrary.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimTrace.h"
#include "Kismet/KismetMathLibrary.h"

static const FVector DefaultHeadIKAxis(0.f, 1.f, 0.f);
static const FVector DefaultLookUpAxis(1.f, 0.f, 0.f);

/////////////////////////////////////////////////////
// FAnimNode_HeadIK

FAnimNode_HeadIK::FAnimNode_HeadIK()
	: HeadIKLocation(FVector(100.f, 0.f, 0.f))
	, HeadIK_Axis(DefaultHeadIKAxis)
	, bUseLookUpAxis(false)
	, LookUp_Axis(DefaultLookUpAxis)
	, ApplyAlpha(0.f)
	, HeadIKClamp(0.f)
	, InterpolationTime(0.f)
	, InterpolationTriggerThreashold(0.f)
	, bEnableIK(false)
	, InterpolationSpeedIn(0.5f)
	, InterpolationSpeedOut(2.f)
	, CurrentHeadIKLocation(ForceInitToZero)
	, CurrentTargetLocation(ForceInitToZero)
	, PreviousTargetLocation(ForceInitToZero)
	, AccumulatedInterpoolationTime(0.f)
	, CachedCurrentTargetLocation(ForceInitToZero)
{
	NeckBonesAlpha.Add(0.5f);
	NeckBonesAlpha.Add(0.2f);
}

void FAnimNode_HeadIK::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(GatherDebugData)
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += "(";
	AddDebugNodeData(DebugLine);
	if (HeadIKTarget.HasValidSetup())
	{
		DebugLine += FString::Printf(TEXT(" Bone: %s, Look At Target: %s, Look At Location: %s, Target Location : %s)"), *BoneToModify.BoneName.ToString(), *HeadIKTarget.GetTargetSetup().ToString(), *HeadIKLocation.ToString(), *CachedCurrentTargetLocation.ToString());
	}
	else
	{
		DebugLine += FString::Printf(TEXT(" Bone: %s, Look At Location : %s, Target Location : %s)"), *BoneToModify.BoneName.ToString(), *HeadIKLocation.ToString(), *CachedCurrentTargetLocation.ToString());
	}

	DebugData.AddDebugItem(DebugLine);

	ComponentPose.GatherDebugData(DebugData);
}

float FAnimNode_HeadIK::AlphaToBlendType(float InAlpha)
{
	return FMath::Clamp<float>(FMath::InterpEaseInOut<float>(0.f, 1.f, InAlpha, 2), 0.f, 1.f);
}

void FAnimNode_HeadIK::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(EvaluateSkeletalControl_AnyThread)
	check(OutBoneTransforms.Num() == 0);

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FCompactPoseBoneIndex ModifyBoneIndex = BoneToModify.GetCompactPoseIndex(BoneContainer);
	FTransform InitialBoneTransform = Output.Pose.GetComponentSpaceTransform(ModifyBoneIndex);
	FTransform ComponentBoneTransform = InitialBoneTransform;

	// get target location
	FTransform TargetTransform = HeadIKTarget.GetTargetTransform(HeadIKLocation, Output.Pose, Output.AnimInstanceProxy->GetComponentTransform());
	FVector TargetLocationInComponentSpace = TargetTransform.GetLocation();
	
	FVector OldCurrentTargetLocation = CurrentTargetLocation;
	FVector NewCurrentTargetLocation = TargetLocationInComponentSpace;

	if ((NewCurrentTargetLocation - OldCurrentTargetLocation).SizeSquared() > InterpolationTriggerThreashold*InterpolationTriggerThreashold)
	{
		if (AccumulatedInterpoolationTime >= InterpolationTime)
		{
			// reset current Alpha, we're starting to move
			AccumulatedInterpoolationTime = 0.f;
		}

		PreviousTargetLocation = OldCurrentTargetLocation;
		CurrentTargetLocation = NewCurrentTargetLocation;
	}
	else if (InterpolationTriggerThreashold == 0.f)
	{
		CurrentTargetLocation = NewCurrentTargetLocation;
	}

	if (InterpolationTime > 0.f)
	{
		float CurrentAlpha = AccumulatedInterpoolationTime / InterpolationTime;

		if (CurrentAlpha < 1.f)
		{
			float BlendAlpha = AlphaToBlendType(CurrentAlpha);
			CurrentHeadIKLocation = FMath::Lerp(PreviousTargetLocation, CurrentTargetLocation, BlendAlpha);
		}
	}
	else
	{
		CurrentHeadIKLocation = CurrentTargetLocation;
	}

#if !UE_BUILD_SHIPPING
	CachedOriginalTransform = ComponentBoneTransform;
	CachedTargetCoordinate = HeadIKTarget.GetTargetTransform(FVector::ZeroVector, Output.Pose, Output.AnimInstanceProxy->GetComponentTransform());
	CachedPreviousTargetLocation = PreviousTargetLocation;
	CachedCurrentHeadIKLocation = CurrentHeadIKLocation;
#endif
	CachedCurrentTargetLocation = CurrentTargetLocation;

	// HeadIK vector
	FVector HeadIKVector = HeadIK_Axis.GetTransformedAxis(ComponentBoneTransform);
	// find look up vector in local space
	FVector LookUpVector = LookUp_Axis.GetTransformedAxis(ComponentBoneTransform);
	// Find new transform from look at info
	FQuat DeltaRotation = AnimationCore::SolveAim(ComponentBoneTransform, CurrentHeadIKLocation, HeadIKVector, bUseLookUpAxis, LookUpVector, HeadIKClamp);
	ComponentBoneTransform.SetRotation(DeltaRotation * ComponentBoneTransform.GetRotation());

	// apply transforms to neck

	const int32 NeckNum = NeckBoneIndices.Num();

	if (NeckNum > 0)
	{
		TArray<FTransform> NeckCS;
		NeckCS.SetNumUninitialized(NeckNum);

		for (int32 i = NeckNum - 1; i != INDEX_NONE; i--)
		{
			NeckCS[i] = Output.Pose.GetComponentSpaceTransform(NeckBoneIndices[i]);

			FQuat targetRot = FQuat::Slerp(NeckCS[i].GetRotation(), ComponentBoneTransform.GetRotation(), NeckBonesAlpha[i]);

			NeckCS[i].SetRotation(targetRot);
			if (i != NeckNum - 1)
			{
				const FTransform LocalTr = Output.Pose.GetLocalSpaceTransform(NeckBoneIndices[i]);
				FTransform TempTr = LocalTr * NeckCS[i + 1];
				NeckCS[i].SetTranslation(TempTr.GetTranslation());
			}

			OutBoneTransforms.Add(FBoneTransform(NeckBoneIndices[i], NeckCS[i]));
		}

		const FTransform LocalTr = Output.Pose.GetLocalSpaceTransform(ModifyBoneIndex);
		ComponentBoneTransform.SetTranslation((LocalTr * NeckCS[0]).GetTranslation());
	}

	// Set New Transform 
	OutBoneTransforms.Add(FBoneTransform(ModifyBoneIndex, ComponentBoneTransform));

#if !UE_BUILD_SHIPPING
	CachedHeadIKTransform = ComponentBoneTransform;
#endif

	TRACE_ANIM_NODE_VALUE(Output, TEXT("Bone"), BoneToModify.BoneName);
	TRACE_ANIM_NODE_VALUE(Output, TEXT("Head IK Target"), HeadIKTarget.HasValidSetup() ? HeadIKTarget.GetTargetSetup() : NAME_None);
	TRACE_ANIM_NODE_VALUE(Output, TEXT("Head IK Location"), HeadIKLocation);
	TRACE_ANIM_NODE_VALUE(Output, TEXT("Target Location"), CachedCurrentTargetLocation);
}

void FAnimNode_HeadIK::EvaluateComponentSpaceInternal(FComponentSpacePoseContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(EvaluateComponentSpaceInternal)
	Super::EvaluateComponentSpaceInternal(Context);
}

bool FAnimNode_HeadIK::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) 
{
	// if both bones are valid
	return (BoneToModify.IsValidToEvaluate(RequiredBones) &&
		// or if name isn't set (use Look At Location) or Look at bone is valid 
		// do not call isValid since that means if look at bone isn't in LOD, we won't evaluate
		// we still should evaluate as long as the BoneToModify is valid even HeadIKBone isn't included in required bones
		(!HeadIKTarget.HasTargetSetup() || HeadIKTarget.IsValidToEvaluate(RequiredBones)) );
}

#if WITH_EDITOR
// can't use World Draw functions because this is called from Render of viewport, AFTER ticking component, 
// which means LineBatcher already has ticked, so it won't render anymore
// to use World Draw functions, we have to call this from tick of actor
void FAnimNode_HeadIK::ConditionalDebugDraw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* MeshComp) const
{
	auto CalculateHeadIKMatrixFromTransform = [this](const FTransform& BaseTransform) -> FMatrix
	{
		FVector TransformedHeadIKAxis = BaseTransform.TransformVector(HeadIK_Axis.Axis);
		const FVector DefaultUpVector = BaseTransform.GetUnitAxis(EAxis::Z);
		FVector UpVector = (bUseLookUpAxis) ? BaseTransform.TransformVector(LookUp_Axis.Axis) : DefaultUpVector;
		// if parallel with up vector, find something else
		if (FMath::Abs(FVector::DotProduct(UpVector, TransformedHeadIKAxis)) > (1.f - ZERO_ANIMWEIGHT_THRESH))
		{
			UpVector = BaseTransform.GetUnitAxis(EAxis::X);
		}

		FVector RightVector = FVector::CrossProduct(TransformedHeadIKAxis, UpVector);
		FMatrix Matrix;
		FVector Location = BaseTransform.GetLocation();
		Matrix.SetAxes(&TransformedHeadIKAxis, &RightVector, &UpVector, &Location);
		return Matrix;
	};

	// did not apply any of LocaltoWorld
	if(PDI && MeshComp)
	{
		FTransform LocalToWorld = MeshComp->GetComponentTransform();
		FTransform ComponentTransform = CachedOriginalTransform * LocalToWorld;
		FTransform HeadIKTransform = CachedHeadIKTransform * LocalToWorld;
		FTransform TargetTrasnform = CachedTargetCoordinate * LocalToWorld;
		FVector BoneLocation = HeadIKTransform.GetLocation();

		// we're using interpolation, so print previous location
		const bool bUseInterpolation = InterpolationTime > 0.f;
		if(bUseInterpolation)
		{
			// this only will be different if we're interpolating
			DrawDashedLine(PDI, BoneLocation, LocalToWorld.TransformPosition(CachedPreviousTargetLocation), FColor(0, 255, 0), 5.f, SDPG_World);
		}

		// current look at location (can be clamped or interpolating)
		DrawDashedLine(PDI, BoneLocation, LocalToWorld.TransformPosition(CachedCurrentHeadIKLocation), FColor::Yellow, 5.f, SDPG_World);
		DrawWireStar(PDI, CachedCurrentHeadIKLocation, 5.0f, FColor::Yellow, SDPG_World);

		// draw current target information
		DrawDashedLine(PDI, BoneLocation, LocalToWorld.TransformPosition(CachedCurrentTargetLocation), FColor::Blue, 5.f, SDPG_World);
		DrawWireStar(PDI, CachedCurrentTargetLocation, 5.0f, FColor::Blue, SDPG_World);

		// draw the angular clamp
		if (HeadIKClamp > 0.f)
		{
			float Angle = FMath::DegreesToRadians(HeadIKClamp);
			float ConeSize = 30.f;
			DrawCone(PDI, FScaleMatrix(ConeSize) * CalculateHeadIKMatrixFromTransform(ComponentTransform), Angle, Angle, 20, false, FLinearColor::Green, GEngine->DebugEditorMaterial->GetRenderProxy(), SDPG_World);
		}

		// draw directional  - HeadIK and look up
		DrawDirectionalArrow(PDI, CalculateHeadIKMatrixFromTransform(HeadIKTransform), FLinearColor::Red, 20, 5, SDPG_World);
		DrawCoordinateSystem(PDI, BoneLocation, HeadIKTransform.GetRotation().Rotator(), 20.f, SDPG_Foreground);
		DrawCoordinateSystem(PDI, TargetTrasnform.GetLocation(), TargetTrasnform.GetRotation().Rotator(), 20.f, SDPG_Foreground);
	}
}
#endif // WITH_EDITOR

void FAnimNode_HeadIK::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(InitializeBoneReferences)
	BoneToModify.Initialize(RequiredBones);
	HeadIKTarget.InitializeBoneReferences(RequiredBones);

	if (!BoneToModify.IsValidToEvaluate(RequiredBones))
	{
		return;
	}

	int32 Num = NeckBonesAlpha.Num();
	NeckBoneIndices.SetNumUninitialized(Num);

	if (Num > 0)
	{
		FCompactPoseBoneIndex IKBoneCompactPoseIndex = BoneToModify.GetCompactPoseIndex(RequiredBones);

		if (IKBoneCompactPoseIndex.GetInt() != INDEX_NONE)
		{
			NeckBoneIndices[0] = RequiredBones.GetParentBoneIndex(IKBoneCompactPoseIndex);
			for (int32 i = 1; i < Num; i++)
			{
				NeckBoneIndices[i] = RequiredBones.GetParentBoneIndex(NeckBoneIndices[i - 1]);
			}
		}
	}
}

void FAnimNode_HeadIK::UpdateInternal(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(UpdateInternal)
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);

	float DeltaTime = Context.GetDeltaTime();

	AccumulatedInterpoolationTime = FMath::Clamp(AccumulatedInterpoolationTime + DeltaTime, 0.f, InterpolationTime);

	if (bEnableIK && ApplyAlpha < 1.f)
	{
		ApplyAlpha += (DeltaTime / InterpolationSpeedIn);
		if (ApplyAlpha > 0.995f) ApplyAlpha = 1.f;
	}
	else if (!bEnableIK && ApplyAlpha > 0.f)
	{
		ApplyAlpha -= (DeltaTime / InterpolationSpeedOut);
		if (ApplyAlpha < 0.005f) ApplyAlpha = 0.f;
	}

	ActualAlpha = FMath::Clamp<float>(FMath::InterpEaseInOut<float>(0.f, 1.f, ApplyAlpha, 2), 0.f, 1.f);
}

void FAnimNode_HeadIK::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
	FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);

	HeadIKTarget.Initialize(Context.AnimInstanceProxy);

	// initialize
	LookUp_Axis.Initialize();
	if (LookUp_Axis.Axis.IsZero())
	{
		UE_LOG(LogAnimation, Warning, TEXT("Zero-length look-up axis specified in HeadIK node. Reverting to default."));
		LookUp_Axis.Axis = DefaultLookUpAxis;
	}
	HeadIK_Axis.Initialize();
	if (HeadIK_Axis.Axis.IsZero())
	{
		UE_LOG(LogAnimation, Warning, TEXT("Zero-length look-at axis specified in HeadIK node. Reverting to default."));
		HeadIK_Axis.Axis = DefaultHeadIKAxis;
	}
}
