#include "MetaHuman/DMetaHumanPlayerController.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Digimate5_4/Public/MetaHuman/DMetaHumanPawnBase.h"

ADMetaHumanPlayerController::ADMetaHumanPlayerController()
{
    
}

void ADMetaHumanPlayerController::BeginPlay()
{
    Super::BeginPlay();

    //AUTH - First timer run
    if (UWorld* World = Cast<UWorld>(GetWorld()))
    {
        World->GetTimerManager().SetTimer(AuthTimerHandle, this, &ADMetaHumanPlayerController::QuitTheApplication, 160.f, false);
    }
}

void ADMetaHumanPlayerController::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
    {
        FModuleManager::Get().LoadModule("WebSockets");
    }

    FString WebSocketAddress = "ws://127.0.0.1:7788";
    TMap<FString, FString> WsUpgradeHeaders;
    WsUpgradeHeaders.Add(TEXT("Host"), TEXT("127.0.0.1:7788"));
    WebSocket = FWebSocketsModule::Get().CreateWebSocket(WebSocketAddress, TEXT("ws"), WsUpgradeHeaders);
    //GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, "Sockets init");

    if (!WebSocket)
    {
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, "Invalid WebSocket");
        return;
    }

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

void ADMetaHumanPlayerController::SendSockedMessage(const FString& MessageString)
{
    if (WebSocket)
    {
        WebSocket->Send(MessageString);
    }
}

void ADMetaHumanPlayerController::ManualyParseMessage(const FString& MessageString)
{
    OnSocketMessage(MessageString);
}

void ADMetaHumanPlayerController::OnSocketMessage(const FString& Message)
{
    ADMetaHumanPawnBase* CurrentPossessedMH = Cast<ADMetaHumanPawnBase>(GetPawn());
    if (!CurrentPossessedMH)
    {
        //If our pawn isn't valid - ignore all events
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, "CurrentPossessedMH Is NULLPTR");
        return;
    }

    GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, Message);

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Message);

    if (!FJsonSerializer::Deserialize(JsonReader, JsonObject))
    {
        //If JSon deserialization failed - ignore all events
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, "Failed to parse JSON.");
        return;
    }

    FString EventType;
    if (!JsonObject->TryGetStringField("type", EventType))
    {
        //If field type isnt contained in parsed JSon file - ignore all events
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, "Missing 'type' field in the JSON.");
        return;
    }



    //Event selection
    if (EventType == "AUTH")
    {        
        if (UWorld* World = Cast<UWorld>(GetWorld()))
        {
            World->GetTimerManager().ClearTimer(AuthTimerHandle);
            World->GetTimerManager().SetTimer(AuthTimerHandle, this, &ADMetaHumanPlayerController::QuitTheApplication, 25.f, false);
        }
    }
    else if(EventType == "PLAY_SOUND")
    {
        FString url;
        FString text;
        if (!JsonObject->TryGetStringField("url", url) || !JsonObject->TryGetStringField("text", text))
        {
            GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, "Missing 'url' or 'text' in JSON.");
            return;
        }

        TArray<FPlaySeparateAnim> AnimationList;
        if (JsonObject->HasField("animations"))
        {
            TArray<TSharedPtr<FJsonValue>> SeparateAnimationsArray;
            SeparateAnimationsArray = JsonObject->GetArrayField("animations");

            for (const auto& AnimationValue : SeparateAnimationsArray)
            {
                TSharedPtr<FJsonObject> AnimationObject = AnimationValue->AsObject();
                if (AnimationObject)
                {
                    float Start = AnimationObject->GetNumberField("start");
                    float End = AnimationObject->GetNumberField("end");

                    FString Animation;
                    AnimationObject->TryGetStringField("animation", Animation);

                    AnimationList.Add(FPlaySeparateAnim(Animation, Start, End));
                }
            }
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

        float LipSyncIntensityHandler = CurrentPossessedMH->LipSyncIntensity;
        if (JsonObject->HasField("intensity"))
        {
            if (!JsonObject->TryGetNumberField("intensity", LipSyncIntensityHandler))
            {
                GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, "Can't get intensity as number");
            }
        }

        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, "speech length: " + FString::FromInt(WordArray.Num()));

        CurrentPossessedMH->SetUpNewAudioToPlay(url, text, WordArray, EmotionsList, AnimationList, LipSyncIntensityHandler);
    }
    else if (EventType == "MOVE_CAMERA")
    {
        FString TargetCameraLocationName;
        float CameraMovementSpeed = 0.f;

        JsonObject->TryGetNumberField("speed", CameraMovementSpeed);

        if (JsonObject->TryGetStringField("name", TargetCameraLocationName))
        {
            CurrentPossessedMH->OnCameraMove(TargetCameraLocationName, CameraMovementSpeed, FVector());
        }
        else if (JsonObject->HasField("Location"))
        {
            float TargetCameraXValue = 0.f;
            float TargetCameraYValue = 0.f;
            float TargetCameraZValue = 0.f;

            if (JsonObject->TryGetNumberField("x_value", TargetCameraXValue) || JsonObject->TryGetNumberField("y_value", TargetCameraYValue) || JsonObject->TryGetNumberField("z_value", TargetCameraZValue))
            {
                CurrentPossessedMH->OnCameraMove(FString(), CameraMovementSpeed, FVector(TargetCameraXValue, TargetCameraYValue, TargetCameraZValue));
            }
        }
    }
    else if (EventType == "SEPARATE_ANIMATION")
    {
        FString SeparateAnimationName;
        if (JsonObject->TryGetStringField("name", SeparateAnimationName))
        {
            CurrentPossessedMH->OnSeparateAnimationReceived(SeparateAnimationName);
        }
    }
    else if (EventType == "CHANGE_OUTFIT")
    {
        FString OutfitName;
        if (JsonObject->TryGetStringField("name", OutfitName))
        {
            CurrentPossessedMH->OnChangeOutfitRequestReceived(OutfitName);
        }
    }
    else if (EventType == "CHANGE_BACKGROUND")
    {
        FString BackgroundURL;
        FString BackgroundName;
        if (JsonObject->TryGetStringField("url", BackgroundURL) || JsonObject->TryGetStringField("name", BackgroundName))
        {
            CurrentPossessedMH->OnChangeBackgroundRequestReceived(BackgroundURL, BackgroundName);
        }
    }
    else if (EventType == "CHANGE_LANGUAGE")
    {
        FString LanguageCodeName;
        if (JsonObject->TryGetStringField("name", LanguageCodeName))
        {
            CurrentPossessedMH->OnChangeLanguageRequestReceived(LanguageCodeName);
        }
    }
    else if (EventType == "CHANGE_MH")
    {
        FString NewMHName;
        if (JsonObject->TryGetStringField("name", NewMHName))
        {
            OnNewMHSpawnRequestReceived(NewMHName);
        }
    }
    else if (EventType == "STOP_SPEAKING")
    {
        CurrentPossessedMH->OnStopSpeakingRequestReceived();
    }
    else if (EventType == "CHANGE_FACE_EMOTION")
    {
        FString NewMHFaceEmotion;
        if (JsonObject->TryGetStringField("name", NewMHFaceEmotion))
        {
            CurrentPossessedMH->OnChangeMHFaceEmotionRequestReceived(NewMHFaceEmotion);
        }
    }
    else if (EventType == "MICROPHONE_ACTIVATED")
    {
        CurrentPossessedMH->OnMicActivateRequestReceived();
    }
    else
    {
        CurrentPossessedMH->OnSocketMessageReceived(EventType);
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, "Unsupported event type: " + EventType);
    }
}

void ADMetaHumanPlayerController::QuitTheApplication()
{
    if (UWorld* World = Cast<UWorld>(GetWorld()))
    {
        UKismetSystemLibrary::QuitGame(World, this, EQuitPreference::Quit, true);
    }
}
