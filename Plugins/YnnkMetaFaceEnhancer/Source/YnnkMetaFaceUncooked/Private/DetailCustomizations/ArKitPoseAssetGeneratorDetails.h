// (c) Yuri N. K. 2022. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "IDetailCustomization.h"

class UArKitPoseAssetGenerator;

class FArKitPoseAssetGeneratorDetails : public IDetailCustomization
{
public:
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	static TSharedRef<IDetailCustomization> MakeInstance();

	bool CanGenerate() const;
	FReply OnGenerateClick() const;

protected:
	TObjectPtr<UArKitPoseAssetGenerator> ModifiedObject;
};