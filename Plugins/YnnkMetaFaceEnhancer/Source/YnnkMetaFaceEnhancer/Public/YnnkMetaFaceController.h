// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MetaFaceFunctionLibrary.h"
#include "MetaFaceTypes.h"
#include "Runtime/Launch/Resources/Version.h"
#include "HAL/CriticalSection.h"
#include "YnnkMetaFaceController.generated.h"

class UYnnkVoiceLipsyncData;
class UYnnkLipsyncController;
class USkeletalMeshComponent;
class UYnnkRemoteClient;
class UNeuralProcessWrapper;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAnimationBuildingResult, const UYnnkVoiceLipsyncData*, LipsyncData, bool, bResult);

/** Separate lip-sync and facial animations */
USTRUCT(BlueprintType)
struct YNNKMETAFACEENHANCER_API FFacialAnimCollection
{
	GENERATED_USTRUCT_BODY()

	// Lip-sync is converted to skeleton curves
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MH Lipsync Viseme")
	FMHFacialAnimation LipSync;

	// Facial animation is stored in ArKit curves (to avoid mixing)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MH Lipsync Viseme")
	FMHFacialAnimation FacialAnimation;
};

/** Set of curves to control eye */
USTRUCT(BlueprintType)
struct FMFEyeControllerSetup
{
	GENERATED_USTRUCT_BODY()

	// Eye bone name
	UPROPERTY(EditAnywhere, Category = "MH Eye Curves Set")
	FName BoneName;

	// Look down
	UPROPERTY(EditAnywhere, Category = "MH Eye Curves Set")
	FName LookDown;

	// Look right
	UPROPERTY(EditAnywhere, Category = "MH Eye Curves Set")
	FName LookLeft;

	// Look left
	UPROPERTY(EditAnywhere, Category = "MH Eye Curves Set")
	FName LookRight;

	// Look up
	UPROPERTY(EditAnywhere, Category = "MH Eye Curves Set")
	FName LookUp;

	// Copy the same values to "EyeLookDownLeft", "EyeLookInLeft", "EyeLookOutLeft", "EyeLookUpLeft" and "EyeLookDownRight", "EyeLookInRight", "EyeLookOutRight", "EyeLookUpRight"
	UPROPERTY(EditAnywhere, meta=(DisplayName="Propagate to AR Curves"), Category = "MH Eye Curves Set")
	bool bPropagateToARCurves;

	FMFEyeControllerSetup() : bPropagateToARCurves(false) {};

	FMFEyeControllerSetup(const FName& Bone, const FName& Down, const FName& Left, const FName& Right, const FName& Up)
		: BoneName(Bone), LookDown(Down), LookLeft(Left), LookRight(Right), LookUp(Up), bPropagateToARCurves(false)
	{}
};

/**
* This component controls MetaHuman facial animation and enhanced lip-sync
*/
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class YNNKMETAFACEENHANCER_API UYnnkMetaFaceController : public UActorComponent
{
	GENERATED_BODY()

public:
	UYnnkMetaFaceController();

	/* UActorComponent overrides */
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	/* end UActorComponent overrides */
	//~ Begin UObject Interface.
	virtual void BeginDestroy() override;
	//~ End UObject Interface.

	/** Name of skeletal mesh component containing MetaHuamn head (Face_Archetype_Skeleton) */
	UPROPERTY(EditAnywhere, Category = "Setup")
	FName HeadComponentName;

	/** Name of skeletal mesh component containing MetaHuamn head (Face_Archetype_Skeleton) */
	UPROPERTY(EditAnywhere, Category = "Setup")
	FName BodyComponentName;

	/** Bone and four curves to control right eye */
	UPROPERTY(EditAnywhere, Category = "Setup")
	FMFEyeControllerSetup EyeRightSetup;

	/** Bone and four curves to control left eye */
	UPROPERTY(EditAnywhere, Category = "Setup")
	FMFEyeControllerSetup EyeLeftSetup;

	/** Max rotation of eye (in degrees) */
	UPROPERTY(EditAnywhere, Category = "Setup")
	float MaxEyeRotationRightLeft = 60.f;

	/** Max rotation of eye (in degrees) */
	UPROPERTY(EditAnywhere, Category = "Setup")
	float MaxEyeRotationUpDown = 40.f;

	/**
	* MetaHumans/Common/Common/Mocap/mh_arkit_mapping_pose - pose asset to convert curves from ArKit to skeleton curves
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "AR Facial Curves Pose Asset"), Category = "Setup")
	UPoseAsset* ArKitCurvesPoseAsset;

	/** Intensity of facial animation played by controller */
	UPROPERTY(EditAnywhere, Category = "Generation")
	float EmotionsIntensity;

	/**
	* Intensity of pre-defined visemes (applid with LipsyncNeuralIntensity < 1)
	*/
	UPROPERTY(EditAnywhere, Category = "Generation")
	float VisemeApplyAlpha;

	/**
	* Alpha to blend form neural net to defined visemes.
	* Note: blending to Ynnk Voice LipSync in animation blueprint is preferreable.
	*/
	UPROPERTY(EditAnywhere, Category = "Generation")
	float LipsyncNeuralIntensity;

	/**
	* Increase to make lip-sync animation more smooth (and lose details)
	*/
	UPROPERTY(EditAnywhere, Category = "Generation")
	float LipsyncSmoothness;

	/**
	* Increase to make facial animation/emotions more smooth (and lose details)
	*/
	UPROPERTY(EditAnywhere, Category = "Generation")
	float FacialAnimationSmoothness;

	/**
	* Keep Frown curves not less then smile curves
	* (Enable this checkbox for CC3/CC4 character)
	*/
	UPROPERTY(EditAnywhere, Category = "Generation")
	bool bBalanceSmileFrownCurves;

	/**
	* Build animation in async thread (disable only at powerful PC)
	* Ignored with remote animation builder enabled
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	bool bAsyncAnimationBuilder;

	/** Max allowed in animation rotation of eye (in degrees) */
	UPROPERTY(EditAnywhere, Category = "Generation")
	float AllowedEyeRotationRightLeft = 60.f;

	/** Max allowed in animation rotation of eye (in degrees) */
	UPROPERTY(EditAnywhere, Category = "Generation")
	float AllowedEyeRotationUpDown = 40.f;

	/**
	* Should automatically generate lip-sync animation for Speak/SpeakEx calls?
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Play")
	bool bApplyLipsyncToSpeak;

	/**
	* Should automatically generate facial animation for Speak/SpeakEx calls?
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Play")
	bool bApplyFacialAnimationToSpeak;

	/**
	* Should expand facial animation to simple skeleton curves?
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Play")
	bool bLipSyncToSkeletonCurves;

	/**
	* Should expand facial animation to simple skeleton curves?
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Play")
	bool bFacialAnimationToSkeletonCurves;

	/**
	* Bake YnnkVoice + MF LipSync + MF FacialAnimation to single curves set
	* bLipSyncToSkeletonCurves and bFacialAnimationToSkeletonCurves shoule be enabled
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bLipSyncToSkeletonCurves && bFacialAnimationToSkeletonCurves"), Category = "Play")
	bool bAutoBakeAnimation = false;

	/** Ratio between YnnkLipsync and MetaFace LipSync in baked animation data. By default, 25% of YnnkLipsync and 75% of MetaFaceEnhancer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bAutoBakeAnimation", DisplayName="Ynnk to MetaFace Bake Ratio", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"), Category = "Play")
	float BakedYnnkToMetaFaceRatio = 0.25f;

	/** Intensity of lips animation in baked animation data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bAutoBakeAnimation"), Category = "Play")
	float BakedLipSyncIntensity = 1.f;

	/** Intensity of lips animation in baked animation data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bAutoBakeAnimation"), Category = "Play")
	float BakedFacialAnimationIntensity = 1.f;

	/**
	* Should use extra animation tracks saved in played UYnnkVoiceLipsyncData asset for lip-sync and facial animation?
	* Enable this option to use pre-saved facial animation.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Play")
	bool bUseExtraAnimationFromLipsyncDataAsset;

	/* Interpolation speed for eyes when eyes are animated (see EyesControllerType) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Play")
	float EyeMovementSpeed;

	/** Draw debug geometry */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawDebug;

	/** Write debug info in log */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bLogDebug = false;

	/**
	* Active lip-sync animation. Use it in animation blueprint
	*/
	UPROPERTY(BlueprintReadWrite, Category = "Ynnk MetaFace Controller")
	FMHFacialAnimation CurrentLipsync;

	/**
	* Active facial animation. Use it in animation blueprint
	*/
	UPROPERTY(BlueprintReadWrite, Category = "Ynnk MetaFace Controller")
	FMHFacialAnimation CurrentFaceAnim;

	/**
	* Active eyes animation. Use it in animation blueprint
	*/
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk MetaFace Controller")
	TMap<FName, float> CurrentEyesState;

	/**
	* Active baked animation without eyes (i. e. Ynnk Voice Lip-Sync + MF Lip-Sync + MF Facial Animation
	* bAutoBakeAnimation should be enabled
	*/
	UPROPERTY(BlueprintReadWrite, Category = "Ynnk MetaFace Controller")
	TMap<FName, float> CurrentBakedFaceFrame;

	/**
	* Playing time for an active audio
	*/
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk MetaFace Controller")
	float PlayTime;

	/**
	* Is using remote (via network) server to build animation?
	*/
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk MetaFace Controller")
	bool bUseRemoteBuilder;

	/**
	* Program of eyes controller, see
	* SetEyesMovementEnabled
	* SetEyesTarget
	*/
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk MetaFace Controller")
	EEyesControlType EyesControllerType;

	/* Rotation of eye relative to head in degrees */
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk MetaFace Controller")
	FVector2D EyeRotation_Right;

	/* Rotation of eye relative to head in degrees */
	UPROPERTY(BlueprintReadOnly, Category = "Ynnk MetaFace Controller")
	FVector2D EyeRotation_Left;

	/**
	* Called when animation is created after BuildFacialAnimationData
	*/
	UPROPERTY(BlueprintAssignable, Category = "Ynnk MetaFace Controller")
	FAnimationBuildingResult OnAnimationBuildingComplete;

	/**
	* Request to build animation and save it to cache for specified UYnnkVoiceLipsyncData asset
	*/
	UFUNCTION(BlueprintCallable, Category = "Ynnk MetaFace Controller")
	bool BuildFacialAnimationData(UYnnkVoiceLipsyncData* LipsyncData, bool bCreateLipSync, bool bCreateFacialAnimation);

	/**
	* Speak VoiceLipsyncData through UYnnkLipsyncController with forced generation of lip-sync and facial animation
	*/
	UFUNCTION(BlueprintCallable, Category = "Ynnk MetaFace Controller")
	void SpeakEx(UYnnkVoiceLipsyncData* VoiceLipsyncData, class USoundWave* Sound = nullptr, float SoundOffset = 0.f);

	/**
	* Speak VoiceLipsyncData through UYnnkLipsyncController with forced generation of lip-sync and facial animation
	*/
	UFUNCTION(BlueprintCallable, Category = "Ynnk MetaFace Controller")
	void Speak(UYnnkVoiceLipsyncData* VoiceLipsyncData);

	/**
	* Connect to server to build animation via network
	*/
	UFUNCTION(BlueprintCallable, Category = "Ynnk MetaFace Controller")
	bool InitializeRemoteAnimationBuilder(UYnnkRemoteClient*& RemoteConnectionClient, FString IPv4 = TEXT("127.0.0.1"), int32 Port = 7575);

	/**
	* Disconnect from remote animation builder and use local (PC/Windows only) builder
	*/
	UFUNCTION(BlueprintCallable, Category = "Ynnk MetaFace Controller")
	void DisconnectFromRemoteBuilder();

	/**
	* Enable/disable remote animation builder (should be connected by InitializeRemoteAnimationBuilder) without disconnecting
	*/
	UFUNCTION(BlueprintCallable, Category = "Ynnk MetaFace Controller")
	bool SetUseRemoteAnimationBuilder(bool bNewIsEnabled);

	/**
	* Enable eyes live movement or disable movement/current target
	*/
	UFUNCTION(BlueprintCallable, Category = "Ynnk MetaFace Controller")
	void SetEyesMovementEnabled(bool bNewIsEnabled);

	/**
	* Set eyes look-at target, TargetComponent has a priority
	*/
	UFUNCTION(BlueprintCallable, Category = "Ynnk MetaFace Controller")
	void SetEyesTarget(FVector TargetLocation, USceneComponent* TargetComponent = nullptr);

	/**
	* Is component initialized and ready to work?
	*/
	UFUNCTION(BlueprintPure, Category = "Ynnk MetaFace Controller")
	bool IsInitialized() const;

	/**
	* Has valid animation in cache for specified UYnnkVoiceLipsyncData asset?
	*/
	UFUNCTION(BlueprintPure, Category = "Ynnk MetaFace Controller")
	void HasValidAnimation(const UYnnkVoiceLipsyncData* LipsyncData, bool& bLipsyncIsValid, bool& bFacialAnimationIsValid) const;
	
	/** Get RemoteClient object */
	UFUNCTION(BlueprintPure, Category = "Ynnk MetaFace Controller")
	UYnnkRemoteClient* GetRemoteConnectionClient() const;

protected:
	// Pointer to UYnnkLipsyncController in the same actor
	UPROPERTY()
	UYnnkLipsyncController* LipsyncController;

	// Pointer to MetaHuman Head
	UPROPERTY()
	USkeletalMeshComponent* HeadMesh;

	// Pointer to MetaHuman Body
	UPROPERTY()
	USkeletalMeshComponent* BodyMesh;

	// Currently processed/speaked lipsync data
	UPROPERTY()
	UYnnkVoiceLipsyncData* ProcessedLipsyncData;

	// Object to send requests to remote server
	UPROPERTY()
	UYnnkRemoteClient* RemoteClient;

	// Set of animations generated in runtime
	UPROPERTY()
	TMap<FName, FFacialAnimCollection> FaceAnimations;

	// Should play voice/animation when building is complete?
	bool bDelayedSpeak;
	USoundWave* DelayedSpeak_SoundWave;
	float  DelayedSpeak_TimeOffset;

	UPROPERTY()
	FVector EyesTargetLocation;

	UPROPERTY()
	USceneComponent* EyesTargetComponent;

	UPROPERTY()
	float EyesNextUpdateTime;

	UPROPERTY()
	FVector2D EyeRotation_Target;

	UPROPERTY()
	float EyesTargetAlpha;

	// Async lipsync generation
	static FCriticalSection AsyncBuildMutex;
	bool bAsyncWorkerIsActive = false;
	bool bExecutionInterrupted = false;

	UPROPERTY()
	float FacialAnimationPauseDuration;

	// ArKit curves used for lip-sync
	// we keep ithen to apply intensity to baked animation
	UPROPERTY()
	TArray<FName> LipSyncARCurvesSet;

	void AsyncBuildAnimation(UYnnkVoiceLipsyncData* LsData);

	// Used to get a result from UAsyncAnimBuilder
	UFUNCTION()
	void OnAsyncBuilder_AnimationCreated(const TMap<FName, FSimpleFloatCurve>& LipsyncData, const TMap<FName, FSimpleFloatCurve>& FacialAnimationData);
	UFUNCTION()
	void OnAsyncBuilder_RequestInterrupted();

	// Used to get a result from remote server
	UFUNCTION()
	void OnRemoteClient_ResponseReceived(int32 RequestID, const FString& Command, const FString& JsonPacket);

	// play from cache (if exists) with YnnkLipsyncController
	UFUNCTION()
	void OnLipsyncController_StartSpeaking(UYnnkVoiceLipsyncData* PhraseAsset);

	// interrupt current phrase
	UFUNCTION()
	void OnLipsyncController_SpeakingInterrupted(UYnnkVoiceLipsyncData* PhraseAsset);

	UFUNCTION()
	void FillEyeAnimationCurves();

	bool CheckAnimationCurvesSetIsArKit(const TMap<FName, FSimpleFloatCurve>& Animation, bool bLipSyncCurves) const;

	UNeuralProcessWrapper* GetNeuralProcessor() const;
};
