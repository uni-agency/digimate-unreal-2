// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

#include "YnnkMetaFaceUncooked.h"
#include "Modules/ModuleManager.h"
#include "ISettingsModule.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "YnnkMetaFaceSettings.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Animation/AnimSequence.h"
#include "Animation/PoseAsset.h"
#include "ContentBrowserModule.h"
#include "YnnkVoiceLipsyncData.h"
#include "TickableNotification.h"
#include "ScopedTransaction.h"
#include "AssetRegistry/AssetData.h"
#include "AsyncAnimBuilder.h"
#include "IContentBrowserSingleton.h"
#include "LevelEditor.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AnimationBlueprintLibrary.h"
#include "MetaFaceEditorFunctionLibrary.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/AnimDataModel.h"
#include "ArKitPoseAssetGenerator.h"
#include "DetailCustomizations/ArKitPoseAssetGeneratorDetails.h"
#include "ToolMenus.h"
#include "PropertyEditorModule.h"
#include "YnnkLipsyncSettings.h"
#include "Runtime/Launch/Resources/Version.h"

#define LOCTEXT_NAMESPACE "FYnnkMetaFaceUncooked"

void FYnnkMetaFaceMenuCommands::RegisterCommands()
{
	UI_COMMAND(OpenARPoseAssetGenerator, "AR Facial Curves PoseAsset Builder", "Bring up AR Facial Curves PoseAsset Builde", EUserInterfaceActionType::Button, FInputChord());
}

void FYnnkMetaFaceUncooked::StartupModule()
{
	FYnnkMetaFaceMenuCommands::Register();

	// Register plugin settings
	ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
	SettingsModule.RegisterSettings("Project", "Plugins", "Ynnk MetaFace",
		NSLOCTEXT("YnnkMetaFace", "YnnkMetaFace", "Ynnk MetaFace"),
		NSLOCTEXT("YnnkMetaFace", "YnnkMetaFaceConfig", "Configure settings for MetaHuman facial animation"),
		GetMutableDefault<UYnnkMetaFaceSettings>());

	// Details customization
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.RegisterCustomClassLayout(UArKitPoseAssetGenerator::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FArKitPoseAssetGeneratorDetails::MakeInstance));

	CommandList = MakeShareable(new FUICommandList);

	// Add Asset Menu item
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.GetAllAssetViewContextMenuExtenders().Add(
		FContentBrowserMenuExtender_SelectedAssets::CreateLambda([this](const TArray<FAssetData>& SelectedAssets)
	{
		TSharedRef<FExtender> Extender = MakeShared<FExtender>();

		if (SelectedAssets.ContainsByPredicate([](const FAssetData& AssetData) { return AssetData.GetClass() == UYnnkVoiceLipsyncData::StaticClass(); }))
		{
			// Refresh Lipsync Asset
			Extender->AddMenuExtension(
				"GetAssetActions",
				EExtensionHook::After,
				CommandList,
				FMenuExtensionDelegate::CreateLambda([this, SelectedAssets](FMenuBuilder& MenuBuilder)
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("YnnkMetaFaceGenerate", "Generate MetaFace Animation"),
					LOCTEXT("YnnkMetaFaceGenerateToolTip", "Create enhanced MetaHuman lip-sync and facial animation and save it in the selected asset(s)"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateRaw(this, &FYnnkMetaFaceUncooked::SaveMetaFaceAnimation, SelectedAssets)));
			}));

			// Create UAnimSequence
			Extender->AddMenuExtension(
				"GetAssetActions",
				EExtensionHook::After,
				CommandList,
				FMenuExtensionDelegate::CreateLambda([this, SelectedAssets](FMenuBuilder& MenuBuilder)
			{
				FText Label = LOCTEXT("YnnkAnimSequence", "Create Animation Sequence with MetaFace...");

				MenuBuilder.AddMenuEntry(
					Label,
					LOCTEXT("YnnkCreateAnimSequenceToolTip", "Create animation sequence with meta-face lipsync curves and emotional facial animation"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateRaw(this, &FYnnkMetaFaceUncooked::CreateAnimSequenceAssets, SelectedAssets)));
			}));
		}

		return Extender;
	}));
	ContentBrowserMenuExtenderHandle = ContentBrowserModule.GetAllAssetViewContextMenuExtenders().Last().GetHandle();

	CommandList->MapAction(
		FYnnkMetaFaceMenuCommands::Get().OpenARPoseAssetGenerator,
		FExecuteAction::CreateRaw(this, &FYnnkMetaFaceUncooked::OpenPoseAssetBuilder),
		FCanExecuteAction());
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FYnnkMetaFaceUncooked::RegisterMenus));
}

void FYnnkMetaFaceUncooked::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FYnnkMetaFaceMenuCommands::Unregister();

	// Details Customizatoin
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomClassLayout(UArKitPoseAssetGenerator::StaticClass()->GetFName());

	FContentBrowserModule* ContentBrowserModule = FModuleManager::GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser"));
	if (ContentBrowserModule && ContentBrowserMenuExtenderHandle.IsValid())
	{
		ContentBrowserModule->GetAllAssetViewContextMenuExtenders().RemoveAll(
			[this](const FContentBrowserMenuExtender_SelectedAssets& InDelegate)
		{
			return ContentBrowserMenuExtenderHandle == InDelegate.GetHandle();
		}
		);
	}
}

void FYnnkMetaFaceUncooked::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection* Section = Menu->FindSection("Custom");
			if (!Section)
			{
				Section = &Menu->AddSection("Custom", NSLOCTEXT("MainAppMenu", "CustomMenuHeader", "Custom"));
			}

			Section->AddMenuEntryWithCommandList(FYnnkMetaFaceMenuCommands::Get().OpenARPoseAssetGenerator, CommandList,
				LOCTEXT("ArKitPoseAssetBuilder", "AR Facial Curves PoseAsset Builder"),
				LOCTEXT("ArKitPoseAssetBuilderToolTip", "Create Pose Asset with AR facial curves for CC3/CC4 or custom skeletal mesh"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "CollisionAnalyzer.TabIcon"));
		}
	}
}

void FYnnkMetaFaceUncooked::OpenPoseAssetBuilder()
{
	ArKitPoseAssetGenerator = NewObject<UArKitPoseAssetGenerator>(UArKitPoseAssetGenerator::StaticClass());

	//ToolInstance->AddToRoot();
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	TArray<UObject*> ObjectsToView;
	ObjectsToView.Add(ArKitPoseAssetGenerator.Get());
	TSharedRef<SWindow> Window = PropertyModule.CreateFloatingDetailsView(ObjectsToView, /*bIsLockeable=*/ true);
	Window->SetTitle(LOCTEXT("VisemesPoseAssetBuilder", "Visemes Pose Asset Builder"));
	Window->SetOnWindowClosed(FOnWindowClosed::CreateRaw(this, &FYnnkMetaFaceUncooked::OnPoseAssetGeneratorClosed));
}

void FYnnkMetaFaceUncooked::OnPoseAssetGeneratorClosed(const TSharedRef<SWindow>& Window)
{
	ArKitPoseAssetGenerator->MarkAsGarbage();
	ArKitPoseAssetGenerator = nullptr;
}

void FYnnkMetaFaceUncooked::CreateAnimSequenceAssets(TArray<FAssetData> SelectedAssets)
{
	USkeleton* TargetSkeleton = Cast<USkeleton>(PickAssetOfClass(FText::FromString(TEXT("Select Target Skeleton (eg. Face_Archetype_Skeleton)...")), USkeleton::StaticClass()));
	if (!IsValid(TargetSkeleton))
	{
		return;
	}

	UPoseAsset* ArKitCurvesAsset = Cast<UPoseAsset>(PickAssetOfClass(FText::FromString(TEXT("Select PoseAsset with AR facial curves (mh_arkit_mapping_pose)...")), UPoseAsset::StaticClass()));
	if (!IsValid(ArKitCurvesAsset))
	{
		return;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	for (const auto& DataAsset : SelectedAssets)
	{
		const UYnnkVoiceLipsyncData* DataLS = Cast<UYnnkVoiceLipsyncData>(DataAsset.GetAsset());
		if (!DataLS)
		{
			UE_LOG(LogTemp, Log, TEXT("[CreateAnimSequenceAssets] Can't cast %s to UYnnkVoiceLipsyncData"), *DataAsset.GetAsset()->GetName());
			continue;
		}

		FString InAssetPath = FPaths::GetPath(DataLS->GetPathName());
		FString ValidatedAssetName = DataLS->GetFName().ToString() + TEXT("_MFAnim");

		const FString NewAssetName = ValidatedAssetName;
		UObject* NewObject = AssetTools.CreateAsset(
			NewAssetName,
			InAssetPath,
			UAnimSequence::StaticClass(), nullptr /*, factory*/);

		UAnimSequence* const NewSequence = Cast<UAnimSequence>(NewObject);

		if (NewSequence)
		{
			const int32 SampleRate = 30;

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(NewSequence);
			// record first frame
			// set skeleton
			NewSequence->SetSkeleton(TargetSkeleton);
			NewSequence->GetController().InitializeModel();
			NewSequence->GetController().SetNumberOfFrames(1);
			NewSequence->GetController().NotifyPopulated();

			// now init data model

			IAnimationDataController& Controller = NewSequence->GetController();
			const FFrameRate ResampleFrameRate(SampleRate, 1);

			// Initialize data model
			Controller.OpenBracket(LOCTEXT("ImportAnimation_Bracket", "Creating Animation"));

				//This destroy all previously imported animation raw data
				Controller.ResetModel();
				// First set frame rate
				Controller.SetFrameRate(ResampleFrameRate);

				// if you have one pose(thus 0.f duration), it still contains animation, so we'll need to consider that as MINIMUM_ANIMATION_LENGTH time length
				const FFrameNumber NumberOfFrames = ResampleFrameRate.AsFrameNumber(DataLS->GetDuration());
				Controller.SetNumberOfFrames(FGenericPlatformMath::Max<int32>(NumberOfFrames.Value, 1), true);
				Controller.NotifyPopulated();

			Controller.CloseBracket();


			// create curves in animation sequence
			// using default parameters
			TMap<FName, FSimpleFloatCurve> BakedData;

			FString CurveFilter = TEXT("");
			const auto YnnkSettings = GetDefault<UYnnkLipsyncSettings>();
			if (YnnkSettings)
			{
				CurveFilter = YnnkSettings->AnimationSettings.CurveNameFilter;
			}

			if (UMetaFaceEditorFunctionLibrary::BakeFacialAnimation(DataLS, BakedData, ArKitCurvesAsset, true, 1.f, 0.72f, 0.28f, SampleRate, EYnnkBlendType::YBT_Additive, true, true, CurveFilter))
			{
				UE_LOG(LogTemp, Log, TEXT("Creating curves in animation asset..."));
				Controller.OpenBracket(LOCTEXT("ImportAnimation_CreateCurves", "Creating Curves"));
				for (const auto& Curve : BakedData)
				{
					FAnimationCurveIdentifier CurveId(Curve.Key, ERawCurveTrackTypes::RCT_Float);
					if (!NewSequence->GetDataModel()->FindFloatCurve(CurveId))
					{
						Controller.AddCurve(CurveId, AACF_DefaultCurve, false);
					}
				}
				Controller.CloseBracket();

				UE_LOG(LogTemp, Log, TEXT("Filling curves in animation asset..."));
				Controller.OpenBracket(LOCTEXT("ImportAnimation_FillCurves", "Filling Curves"));
				for (const auto& Curve : BakedData)
				{
					UMetaFaceEditorFunctionLibrary::AddFloatKeysToCurve(NewSequence, Curve.Key, Curve.Value, 0.f, SampleRate, false, false);
				}
				Controller.CloseBracket();
			}
		}
	}
}

void FYnnkMetaFaceUncooked::SaveMetaFaceAnimation(TArray<FAssetData> SelectedAssets)
{
	TArray<UYnnkVoiceLipsyncData*> LipsyncDataSet;
	for (auto& Asset : SelectedAssets)
	{
		UYnnkVoiceLipsyncData* LipsyncData = Cast<UYnnkVoiceLipsyncData>(Asset.GetAsset());
		if (LipsyncData && LipsyncData->PhonemesData.Num() > 0)
		{
			LipsyncDataSet.Add(LipsyncData);
		}
	}

	if (LipsyncDataSet.Num() > 0)
	{
		// Create continuous notification
		if (!ProcessNotification.IsValid())
		{
			ProcessNotification = MakeShareable(new ContinuousTaskNotification::FTickableNotification());
		}
		FString Text = TEXT("Building animation 1/") + FString::FromInt(LipsyncDataSet.Num()) + TEXT("...");
		ProcessNotification->SetDisplayText(FText::FromString(Text));
		ProcessNotification->CreateNotification();

		auto FirstItem = LipsyncDataSet.Pop();

		if (!AnimBuilder)
		{
			AnimBuilder = UAsyncAnimBuilder::CreateAsyncAnimBuilder(FirstItem, true, true);
		}

		if (AnimBuilder)
		{
			AnimBuilder->StartAsQueue(LipsyncDataSet, FAsyncMetaFaceQueueResult::CreateStatic(&FYnnkMetaFaceUncooked::OnMetaFaceResult));
		}
	}
}

void FYnnkMetaFaceUncooked::OnMetaFaceResult(UYnnkVoiceLipsyncData* ProcessedAsset, int32 Processed, int32 Total)
{
	auto ModuleLsEd = FModuleManager::GetModulePtr<FYnnkMetaFaceUncooked>(TEXT("YnnkMetaFaceUncooked"));

	if (ModuleLsEd && ModuleLsEd->ProcessNotification.IsValid())
	{
		if (ModuleLsEd->AnimBuilder)
		{
			FString s = TEXT("Animation added to ") + ProcessedAsset->GetName();
			const FScopedTransaction Transaction(FText::FromString(s));

			ProcessedAsset->Modify();
			ProcessedAsset->ExtraAnimData1 = ModuleLsEd->AnimBuilder->OutLipsyncData;
			ProcessedAsset->ExtraAnimData2 = ModuleLsEd->AnimBuilder->OutFacialAnimationData;
		}

		if (Processed == Total)
		{
			ModuleLsEd->AnimBuilder = nullptr;
			ModuleLsEd->ProcessNotification->DestroyNotification();
		}
		else
		{
			FString Text = TEXT("Building animation ") + FString::FromInt(Processed + 1) + ("/") + FString::FromInt(Total) + TEXT("...");
			ModuleLsEd->ProcessNotification->SetDisplayText(FText::FromString(Text));
		}
	}
}

TObjectPtr<UObject> FYnnkMetaFaceUncooked::PickAssetOfClass(const FText& Title, UClass* Class)
{

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FAssetPickerConfig AssetPickerConfig;
	//AssetPickerConfig.Filter.ClassNames.Add(Class->GetFName());
	//AssetPickerConfig.Filter.ClassPaths.Add(FTopLevelAssetPath(Class));
	AssetPickerConfig.Filter.ClassPaths.Add(FTopLevelAssetPath(Class->GetPathName()));
	AssetPickerConfig.Filter.bRecursiveClasses = false;
	AssetPickerConfig.bAddFilterUI = true;
	AssetPickerConfig.bAllowNullSelection = true;

	/** The delegate that fires when an asset was selected */
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda([this](const FAssetData& AssetData)
	{
		PickedGameAsset = AssetData.GetAsset();
	});
	AssetPickerConfig.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateLambda([&](const FAssetData& AssetData)
	{
		PickerWindow->RequestDestroyWindow();
	});

	/** The default view mode should be a list view */
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

	PickerWindow = SNew(SWindow)
	.Title(Title)
	.ClientSize(FVector2D(500, 600))
	.SupportsMinimize(false)
	.SupportsMaximize(false)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		]
	];

	GEditor->EditorAddModalWindow(PickerWindow.ToSharedRef());
	PickerWindow.Reset();

	return PickedGameAsset;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FYnnkMetaFaceUncooked, YnnkMetaFaceUncooked)
