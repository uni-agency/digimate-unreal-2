
#include "Digimate5_4/Public/MetaHuman/DMetaHumanPawnBase.h"

#include "Sound/SoundWave.h"
#include "WebSocketsModule.h" // Module definition
#include "IWebSocket.h"       // Socket definition
//#include "RuntimeFilesDownloader/Public/FileToMemoryDownloader.h"
#include "YnnkLipSyncFunctionLibrary.h"
#include "RuntimeAudioImporterLibrary.h"

#pragma optimize("", off)

// Sets default values
ADMetaHumanPawnBase::ADMetaHumanPawnBase()
{
    // Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;

}

void ADMetaHumanPawnBase::BeginPlay()
{
    Super::BeginPlay();

    OnFileDownloaded.BindUFunction(this, FName("ImportAudioFromDownloadedFile"));
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

FString ADMetaHumanPawnBase::GetAnimationByTime(float CurrentTime, const TArray<FPlaySeparateAnim>& AnimArray) const
{
    FString CurrentAnimation;

    for (const FPlaySeparateAnim& AnimData : AnimArray)
    {
        if (CurrentTime >= AnimData.Start && CurrentTime <= AnimData.End)
        {
            CurrentAnimation = AnimData.SeparateAnimName;
            break;
        }
    }

    return CurrentAnimation;
}

UYnnkVoiceLipsyncData* ADMetaHumanPawnBase::GetNextLipsyncDataToSkeak()
{
    for (FPlayAudioStruct& Iter : AudioData)
    {
        if (Iter.Position == AudioPlayedPosition)
        {
            if (Iter.LipsyncDataToSpeak)
            {
                if (AudioPlayedPosition == AudioPlayedMaxPos)
                {
                    AudioPlayedPosition = 1;
                    AudioPlayedMaxPos = 0;

                    CurrentLipsyncEmotionsData = Iter.Emotions;
                    CurrentSeparateAnimations = Iter.SeparateAnimations;

                    return Iter.LipsyncDataToSpeak;
                }

                AudioPlayedPosition++;

                CurrentLipsyncEmotionsData = Iter.Emotions;
                CurrentSeparateAnimations = Iter.SeparateAnimations;

                return Iter.LipsyncDataToSpeak;
            }
        }

    }
    
    return nullptr;
}

void ADMetaHumanPawnBase::ClearAudioData()
{
    AudioData.Empty();
}

void ADMetaHumanPawnBase::SetUpNewAudioToPlay(FString& AudioURL, FString& Text, TArray<FSingeWordData>& AudioSinge, TArray<FSingeWordData>& Emotions, TArray<FPlaySeparateAnim>& Animations, float& NewLipSyncIntensity)
{
    DownloadFileByURL(AudioURL, ++AudioPlayedMaxPos);

    AudioData.Add(FPlayAudioStruct(AudioURL, AudioPlayedMaxPos, Text, AudioSinge, Emotions, Animations, NewLipSyncIntensity, nullptr));
}

void ADMetaHumanPawnBase::DownloadFileByURL(FString& URL, int32 Position)
{
    float Timeout{ 0.f };
    FString ContentType{ "" };
    bool bForceByPayload{ true };
    FOnDownloadProgress OnProgress;

    if (!URL.IsEmpty())
    {
        UFileToMemoryDownloader::DownloadFileToMemory(URL, Timeout, ContentType, bForceByPayload, OnProgress, OnFileDownloaded, Position);
    }
}

void ADMetaHumanPawnBase::ImportAudioFromDownloadedFile(const TArray<uint8>& DownloadedContent, EDownloadToMemoryResult Result, UFileToMemoryDownloader* Downloader, int32 Position)
{
    
    URuntimeAudioImporterLibrary* Importer = URuntimeAudioImporterLibrary::CreateRuntimeAudioImporter();
    if (!Importer)
    {
        return;
    }


    if (Result == EDownloadToMemoryResult::Success || Result == EDownloadToMemoryResult::SucceededByPayload)
    {
        Importer->OnResult.AddDynamic(this, &ADMetaHumanPawnBase::OnAudioImported);
        Importer->ImportAudioFromBuffer(DownloadedContent, ERuntimeAudioFormat::Wav, Position);
    }
    else
    {
        return;
    }
}

void ADMetaHumanPawnBase::OnAudioImported(URuntimeAudioImporterLibrary* Importer, UImportedSoundWave* ImportedSoundWave, ERuntimeImportStatus Status, int32 Position)
{
    if (Status == ERuntimeImportStatus::SuccessfulImport)
    {
        for (auto Iter : AudioData)
        {
            if (Iter.Position == Position)
            {
                AddLipsyncGeneratedDataToSpeakData(ImportedSoundWave, Iter.AudioSinge, Position);
            }
        }
    }
    else 
    {
        return;
    }
}

void ADMetaHumanPawnBase::AddLipsyncGeneratedDataToSpeakData(USoundWave* VoiceAsset, TArray<FSingeWordData>& VoiceRecognizedData, int32 Position)
{
    for (FPlayAudioStruct& Iter : AudioData)
    {
        if (Iter.Position == Position)
        {
            Iter.LipsyncDataToSpeak = UYnnkLipSyncFunctionLibrary::CreateLipsyncForRecognizedAudioEx(this, VoiceAsset, EVoiceRecognitionResultFormat::VRD_Words, VoiceRecognizedData);
        }
    }

    if (Position == 1)
    {
        StartFirstAudioToPlay();
    }
}

#pragma optimize("", on)