#pragma once

#include "Sound/SoundWave.h"
#include "WebSocketsModule.h" // Module definition
#include "IWebSocket.h"       // Socket definition
#include "YnnkTypes.h"

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DMetaHumanPawnBase.generated.h"

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
	void OnPlayAudioFile(const FString& AudioURL, const FString& Text, const TArray<FSingeWordData>& AudioSinge, const TArray<FSingeWordData>& Emotions) const;

	UFUNCTION(BlueprintImplementableEvent)
	void OnCameraMove(const FString& TargetCameraLocationName, const float CameraMovementSpeed) const;

	UFUNCTION(BlueprintImplementableEvent)
	void OnSocketMessageReceived(const FString& type) const;

	UFUNCTION(BlueprintPure, Category = "WebSocket")
	FString GetEmotionByTime(float CurrentTime, const TArray<FSingeWordData>& WordArray) const;

	UFUNCTION(BlueprintCallable, Category = "WebSocket")
	void SendSockedMessage(const FString& MessageString);

	UFUNCTION(BlueprintCallable, Category = "WebSocket")
	void ManualyParseMessage(const FString& MessageString);

	void OnSocketMessage(const FString& Message);

	/*FString GetEventTypeByString(ESocketEvent NewEvent);*/

	TSharedPtr<IWebSocket> WebSocket;
};
