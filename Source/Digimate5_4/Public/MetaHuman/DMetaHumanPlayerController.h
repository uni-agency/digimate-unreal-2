#pragma once

#include "WebSocketsModule.h" // Module definition
#include "IWebSocket.h"       // Socket definition



#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DMetaHumanPlayerController.generated.h"

UCLASS()
class DIGIMATE5_4_API ADMetaHumanPlayerController : public APlayerController
{
	GENERATED_BODY()
	

protected:

public:
	ADMetaHumanPlayerController();

	virtual void BeginPlay() override;

	virtual void PostInitializeComponents() override;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void OnNewMHSpawnRequestReceived(const FString& NewSeparateAnimation) const;



	//Websocket parts
	UFUNCTION(BlueprintCallable, Category = "WebSocket")
	void SendSockedMessage(const FString& MessageString);

	UFUNCTION(BlueprintCallable, Category = "WebSocket")
	void ManualyParseMessage(const FString& MessageString);

	void OnSocketMessage(const FString& Message);

	TSharedPtr<IWebSocket> WebSocket;



	//Authentication
	void QuitTheApplication();

	FTimerHandle AuthTimerHandle;

};
