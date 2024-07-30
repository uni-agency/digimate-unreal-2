#pragma once

#include "Sound/SoundWave.h"
#include "WebSocketsModule.h" // Module definition
#include "IWebSocket.h"       // Socket definition
#include "YnnkTypes.h"
#include "YnnkVoiceLipsync\Public\YnnkVoiceLipsyncData.h"

#include "RuntimeFilesDownloader/Public/FileToMemoryDownloader.h"

#include "Digimate5_4/Public/MetaHumanData/DMetaHumanOutfitDataAssetBase.h"

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DMetaHumanPawnBase.generated.h"

USTRUCT()
struct FPlayAudioStruct
{
	GENERATED_USTRUCT_BODY()
public:
	FString AudioURL;
	int32 Position;
	FString Text;
	TArray<FSingeWordData> AudioSinge;
	TArray<FSingeWordData> Emotions;
	FString SeparateAnimation;
	float NewLipSyncIntensity;
	UYnnkVoiceLipsyncData* LipsyncDataToSpeak;
};

UCLASS()
class DIGIMATE5_4_API ADMetaHumanPawnBase : public APawn
{
	GENERATED_BODY()

protected:

public:
	ADMetaHumanPawnBase();

	void BeginPlay() override;

	//MH's EVENTS
	///////////////////////////////////////////////////////
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

	UFUNCTION(BlueprintImplementableEvent)
	void OnChangeLanguageRequestReceived(const FString& NewLanguageName) const;

	UFUNCTION(BlueprintImplementableEvent)
	void OnChangeMHFaceEmotionRequestReceived(const FString& NewMHFaceEmotion) const;

	UFUNCTION(BlueprintImplementableEvent)
	void OnStopSpeakingRequestReceived() const;

	UFUNCTION(BlueprintImplementableEvent)
	void OnMicActivateRequestReceived() const;
	///////////////////////////////////////////////////////

	UFUNCTION(BlueprintPure, Category = "WebSocket")
	FString GetEmotionByTime(float CurrentTime, const TArray<FSingeWordData>& WordArray) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FString, UDMetaHumanOutfitDataAssetBase*> OutfitHandler;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LipSync")
	float LipSyncIntensity;

	 

	///////////////////////////////////////////////////////
	UFUNCTION(BlueprintImplementableEvent)
	void StartFirstAudioToPlay();

	UPROPERTY()
	TArray<FPlayAudioStruct> AudioData;

	int32 AudioPlayedPosition = 1;

	UPROPERTY(BlueprintReadWrite)
	int32 AudioPlayedMaxPos = 0;

	FOnFileToMemoryDownloadCompleteWithPosition OnFileDownloaded;

	UFUNCTION(BlueprintCallable)
	UYnnkVoiceLipsyncData* GetNextLipsyncDataToSkeak();

	UFUNCTION(BlueprintCallable)
	void ClearAudioData();

	UFUNCTION(BlueprintCallable)
	void SetUpNewAudioToPlay(FString& AudioURL, FString& Text, TArray<FSingeWordData>& AudioSinge, TArray<FSingeWordData>& Emotions, FString& SeparateAnimation, float& NewLipSyncIntensity);

	void DownloadFileByURL(FString& URL, int32 Position);

	UFUNCTION()
	void ImportAudioFromDownloadedFile(const TArray<uint8>& DownloadedContent, EDownloadToMemoryResult Result, UFileToMemoryDownloader* Downloader, int32 Position);

	UFUNCTION()
	void OnAudioImported(URuntimeAudioImporterLibrary* Importer, UImportedSoundWave* ImportedSoundWave, ERuntimeImportStatus Status, int32 Position);

	void AddLipsyncGeneratedDataToSpeakData(USoundWave* VoiceAsset, TArray<FSingeWordData>& VoiceRecognizedData, int32 Position);
};