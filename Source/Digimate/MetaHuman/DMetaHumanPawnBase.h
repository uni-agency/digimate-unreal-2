#pragma once

#include "Sound/SoundWave.h"
#include "WebSocketsModule.h" // Module definition
#include "IWebSocket.h"       // Socket definition
#include "YnnkTypes.h"

#include "Digimate/MetaHumanData/DMetaHumanOutfitDataAssetBase.h"

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DMetaHumanPawnBase.generated.h"

enum class EEventTypes;

UCLASS()
class DIGIMATE_API ADMetaHumanPawnBase : public APawn
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

public:
	ADMetaHumanPawnBase();

	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void PostInitializeComponents() override;

	virtual void BeginDestroy() override;



	UFUNCTION(BlueprintImplementableEvent)
	void OnPlayAudioFile(const FString& AudioURL, const FString& Text, const TArray<FSingeWordData>& AudioSinge, const TArray<FSingeWordData>& Emotions, const FString& SeparateAnimation, const float& NewLipSyncIntensity) const;

	UFUNCTION(BlueprintImplementableEvent)
	void OnCameraMove(const FString& TargetCameraLocationName, const float CameraMovementSpeed, const FVector TargetCameraLocationVector) const;

	UFUNCTION(BlueprintImplementableEvent)
	void OnSocketMessageReceived(const FString& type) const;

	UFUNCTION(BlueprintImplementableEvent)
	void OnChangeOutfitRequestReceived(const FString& NewOutfitName) const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void OnChangeBackgroundRequestReceived(const FString& ImageURL, const FString& NewBackgroundName) const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void OnSeparateAnimationReceived(const FString& NewSeparateAnimation) const;

	UFUNCTION(BlueprintPure, Category = "WebSocket")
	FString GetEmotionByTime(float CurrentTime, const TArray<FSingeWordData>& WordArray) const;

	UFUNCTION(BlueprintCallable, Category = "WebSocket")
	void SendSockedMessage(const FString& MessageString);

	UFUNCTION(BlueprintCallable, Category = "WebSocket")
	void ManualyParseMessage(const FString& MessageString);

	void OnSocketMessage(const FString& Message);

	EEventTypes GetEventByName(const FString& Event);

	TSharedPtr<IWebSocket> WebSocket;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FString, UDMetaHumanOutfitDataAssetBase*> OutfitHandler;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LipSync")
	float LipSyncIntensity;
};
