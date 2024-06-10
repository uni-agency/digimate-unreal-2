#pragma once

#include "Digimate/MetaHuman/DMetaHumanPawnBase.h"
#include "DMetaHumanPawnBase.h"

#include "Sound/SoundWave.h"
#include "WebSocketsModule.h" // Module definition
#include "IWebSocket.h"       // Socket definition
#include "YnnkTypes.h"

// Sets default values
ADMetaHumanPawnBase::ADMetaHumanPawnBase()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ADMetaHumanPawnBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ADMetaHumanPawnBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ADMetaHumanPawnBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void ADMetaHumanPawnBase::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    //If actor are HIDDEN in render settings it wont use web socket setup
    if (!IsHidden())
    {
        if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
        {
            FModuleManager::Get().LoadModule("WebSockets");
        }

        FString WebSocketAddress = "ws://127.0.0.1:7788";
        TMap<FString, FString> WsUpgradeHeaders;
        WsUpgradeHeaders.Add(TEXT("Host"), TEXT("127.0.0.1:7788"));
        WebSocket = FWebSocketsModule::Get().CreateWebSocket(WebSocketAddress, TEXT("ws"), WsUpgradeHeaders);
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, "Sockets init");

        // FOR DEBUG ONLY
        //OnPlayAudioFile("https://metahuman-tts.s3.us-east-1.amazonaws.com/Salli_624365786083953974.wav");

        WebSocket->OnConnected().AddLambda([]()
            {
                GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, "Successfully connected");
            });

        WebSocket->OnConnectionError().AddLambda([](const FString& Error)
            {
                GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, Error);
            });

        WebSocket->OnClosed().AddLambda([](int32 StatusCode, const FString& Reason, bool bWasClean)
            {
                GEngine->AddOnScreenDebugMessage(-1, 15.0f, bWasClean ? FColor::Green : FColor::Red, "Connection closed " + Reason);
            });

        WebSocket->OnMessage().AddLambda([this](const FString& MessageString)
            {
                OnSocketMessage(MessageString);
            });

        WebSocket->Connect();
    }
}

void ADMetaHumanPawnBase::BeginDestroy()
{
    //If actor are HIDDEN in render settings it wont use web socket setup
    if (!IsHidden())
    {
        if (WebSocket.IsValid() && WebSocket->IsConnected())

        {
            WebSocket->Close();
        }
    }

    Super::BeginDestroy();
}

FString ADMetaHumanPawnBase::GetEmotionByTime(float CurrentTime, const TArray<FSingeWordData>& WordArray) const
{
    FString CurrentEmotion;

    for (const FSingeWordData& WordData : WordArray)
    {
        if (CurrentTime >= WordData.TimeStart && CurrentTime <= WordData.TimeEnd)
        {
            CurrentEmotion = WordData.Word;
            break;
        }
    }

    return CurrentEmotion;
}

void ADMetaHumanPawnBase::SendSockedMessage(const FString& MessageString)
{
    if (!WebSocket.IsValid() || !WebSocket->IsConnected())
    {
        return;
    }

    WebSocket->Send(MessageString);
}

void ADMetaHumanPawnBase::ManualyParseMessage(const FString& MessageString)
{
    OnSocketMessage(MessageString);
}

void ADMetaHumanPawnBase::OnSocketMessage(const FString& Message)
{
    GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, Message);

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Message);

    if (!FJsonSerializer::Deserialize(JsonReader, JsonObject))
    {
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, "Failed to parse JSON.");
        return;
    }

    FString EventType;
    if (!JsonObject->TryGetStringField("type", EventType))
    {
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, "Missing 'type' field in the JSON.");
        return;
    }

    if (EventType == "PLAY_SOUND")
    {
        FString url;
        FString text;
        if (!JsonObject->TryGetStringField("url", url) || !JsonObject->TryGetStringField("text", text))
        {
            GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, "Missing 'url' or 'text' in JSON.");
            return;
        }

        FString SeparateAnimationName;
        if (JsonObject->HasField("separate_animation"))
        {
            JsonObject->TryGetStringField("separate_animation", SeparateAnimationName);
        }

        if (!JsonObject->HasField("speech"))
        {
            GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, "Missing 'speech' in JSON.");
            return;
        }

        // Extract word information from the "speech" array
        TArray<FSingeWordData> WordArray;
        TArray<TSharedPtr<FJsonValue>> SpeechArray;
        SpeechArray = JsonObject->GetArrayField("speech");

        for (const auto& SpeechValue : SpeechArray)
        {
            TSharedPtr<FJsonObject> SpeechObject = SpeechValue->AsObject();
            if (SpeechObject)
            {
                FString Word;
                if (SpeechObject->TryGetStringField("word", Word))
                {
                    float Start = SpeechObject->GetNumberField("start");
                    float End = SpeechObject->GetNumberField("end");

                    WordArray.Add(FSingeWordData(Word, Start, End));
                }
            }
        }



        if (!JsonObject->HasField("emotions"))
        {
            GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, "Missing 'emotions' in JSON.");
            return;
        }

        TArray<FSingeWordData> EmotionsList;
        TArray<TSharedPtr<FJsonValue>> EmotionsArray;
        EmotionsArray = JsonObject->GetArrayField("emotions");

        for (const auto& EmotionValue : EmotionsArray)
        {
            TSharedPtr<FJsonObject> EmotionObject = EmotionValue->AsObject();
            if (EmotionObject)
            {
                float Start = EmotionObject->GetNumberField("start");
                float End = EmotionObject->GetNumberField("end");
                float AnimIntensity = EmotionObject->GetNumberField("rate");

                FString Emotion;
                EmotionObject->TryGetStringField("emotion", Emotion);

                EmotionsList.Add(FSingeWordData(Emotion, Start, End));
            }
        }

        float LipSyncIntensityHandler = LipSyncIntensity;
        if (JsonObject->HasField("intensity"))
        {
            if (!JsonObject->TryGetNumberField("intensity", LipSyncIntensityHandler))
            {
                GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, "Can't get intensity as number");
            }
        }

        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, "speech length: " + FString::FromInt(WordArray.Num()));

        OnPlayAudioFile(url, text, WordArray, EmotionsList, SeparateAnimationName, LipSyncIntensityHandler);
    }
    else if (EventType == "MOVE_CAMERA")
    {
        FString TargetCameraLocationName;
        float CameraMovementSpeed = 0.f;

        JsonObject->TryGetNumberField("speed", CameraMovementSpeed);

        if (JsonObject->TryGetStringField("name", TargetCameraLocationName))
        {
            OnCameraMove(TargetCameraLocationName, CameraMovementSpeed, FVector());
        }
        else if (JsonObject->HasField("Location"))
        {
            float TargetCameraXValue = 0.f;
            float TargetCameraYValue = 0.f;
            float TargetCameraZValue = 0.f;

            if(JsonObject->TryGetNumberField("x_value", TargetCameraXValue) || JsonObject->TryGetNumberField("y_value", TargetCameraYValue) || JsonObject->TryGetNumberField("z_value", TargetCameraZValue))
            {
                OnCameraMove(FString(), CameraMovementSpeed, FVector(TargetCameraXValue, TargetCameraYValue, TargetCameraZValue));
            }
        }
    }
    else if (EventType == "SEPARATE_ANIMATION")
    {
        FString SeparateAnimationName;
        if (JsonObject->TryGetStringField("name", SeparateAnimationName))
        {
            OnSeparateAnimationReceived(SeparateAnimationName);
        }
    }
    else if (EventType == "CHANGE_OUTFIT")
    {
        FString OutfitName;
        if (JsonObject->TryGetStringField("name", OutfitName))
        {
            OnChangeOutfitRequestReceived(OutfitName);
        }
    }
    else if (EventType == "CHANGE_BACKGROUND")
    {
        FString BackgroundURL;
        FString BackgroundName;
        if (JsonObject->TryGetStringField("url", BackgroundURL) || JsonObject->TryGetStringField("name", BackgroundName))
        {
            OnChangeBackgroundRequestReceived(BackgroundURL, BackgroundName);
        }
        }
    else
    {
        OnSocketMessageReceived(EventType);
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, "Unsupported event type: " + EventType);
    }
}
