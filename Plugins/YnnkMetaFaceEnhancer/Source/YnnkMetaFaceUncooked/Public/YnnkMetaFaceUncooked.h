// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

#pragma once
 
#include "Modules/ModuleManager.h"
#include "Framework/Commands/UICommandList.h"
#include "AssetRegistry/AssetData.h"
#include "TickableNotification.h"
#include "Framework/Commands/Commands.h"
#include "Runtime/Launch/Resources/Version.h"

class UYnnkVoiceLipsyncData;
class UAsyncAnimBuilder;

/** Menu commands for the plugin CurvesCapture tool */
class FYnnkMetaFaceMenuCommands : public TCommands<FYnnkMetaFaceMenuCommands>
{
public:

	FYnnkMetaFaceMenuCommands()
		: TCommands<FYnnkMetaFaceMenuCommands>(TEXT("YnnkMetaFaceEnhancer"), NSLOCTEXT("Contexts", "YnnkMetaFace", "YnnkMetaFace Plugin"), NAME_None, FAppStyle::GetAppStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> OpenARPoseAssetGenerator;
};

class FYnnkMetaFaceUncooked : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// Generate MetaFace animation and save to UYnnkVoiceLipsyncData
	void SaveMetaFaceAnimation(TArray<FAssetData> SelectedAssets);
	void CreateAnimSequenceAssets(TArray<FAssetData> SelectedAssets);

	// Batch generate update
	static void OnMetaFaceResult(UYnnkVoiceLipsyncData* ProcessedAsset, int32 Processed, int32 Total);

private:

	// UYnnkVoiceLipsyncData Content Menu handle
	FDelegateHandle ContentBrowserMenuExtenderHandle;

	// Custom Commands List
	TSharedPtr<FUICommandList> CommandList;
	TObjectPtr<UAsyncAnimBuilder> AnimBuilder;

	// Picked skeleton asset
	TObjectPtr<UObject> PickedGameAsset;
	// Skeleton pick window
	TSharedPtr<SWindow> PickerWindow;
	TObjectPtr<UObject> PickAssetOfClass(const FText& Title, UClass* Class);

	// Notification for batch generation of animation
	TSharedPtr<ContinuousTaskNotification::FTickableNotification> ProcessNotification;

	TObjectPtr<class UArKitPoseAssetGenerator> ArKitPoseAssetGenerator;

	void RegisterMenus();
	void OpenPoseAssetBuilder();
	void OnPoseAssetGeneratorClosed(const TSharedRef<SWindow>& Window);
};