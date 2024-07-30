// (c) Yuri N. K. 2022. All rights reserved.
// ykasczc@gmail.com

#include "ArKitPoseAssetGeneratorDetails.h"
#include "LiveLinkRemapAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/PoseAsset.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Runtime/Launch/Resources/Version.h"
// Target class
#include "ArKitPoseAssetGenerator.h"

#define LOCTEXT_NAMESPACE "ArKitPoseAssetGenerator"

void FArKitPoseAssetGeneratorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	ModifiedObject = nullptr;

	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	if (ObjectsBeingCustomized.Num() != 1)
	{
		return;
	}
	UArKitPoseAssetGenerator* Obj = Cast<UArKitPoseAssetGenerator>(ObjectsBeingCustomized[0].Get());
	if (!Obj)
	{
		return;
	}
	ModifiedObject = Obj;

	// Font
	const FSlateFontInfo& DefaultFont = FAppStyle::GetFontStyle(TEXT("StandardDialog.SmallFont"));

	// Create a commands category
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT("Commands"), FText::GetEmpty(), ECategoryPriority::Important);

	// Command buttons
	Category.AddCustomRow(FText::FromString(TEXT("Commands")))
	.WholeRowContent()
	.HAlign(HAlign_Fill)
	[
		SNew(SButton)
		.ToolTipText(LOCTEXT("BuildARPoseAssetToolTip", "Create anim sequence and pose asset containing AR facial curves using parameters selected below"))
		.IsEnabled(this, &FArKitPoseAssetGeneratorDetails::CanGenerate)
		.OnClicked(this, &FArKitPoseAssetGeneratorDetails::OnGenerateClick)
		[
			SNew(STextBlock)
			.Font(DefaultFont)
			.ColorAndOpacity(FLinearColor::White)
			.Text(LOCTEXT("BuildVisemesPoseAsset", "Create Pose Asset with AR facial curves"))
			.Margin(FMargin(0.f, 4.f))
		]
	];
}

TSharedRef<IDetailCustomization> FArKitPoseAssetGeneratorDetails::MakeInstance()
{
	return MakeShareable(new FArKitPoseAssetGeneratorDetails);
}

bool FArKitPoseAssetGeneratorDetails::CanGenerate() const
{
	// Now we allow to generate psoe assets for raw ArKit curves
	return IsValid(ModifiedObject) && IsValid(ModifiedObject->TargetSkeletalMesh);
}

FReply FArKitPoseAssetGeneratorDetails::OnGenerateClick() const
{
	if (IsValid(ModifiedObject))
	{
		if (ModifiedObject->GeneratePoseAsset())
		{

		}
	}
	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE