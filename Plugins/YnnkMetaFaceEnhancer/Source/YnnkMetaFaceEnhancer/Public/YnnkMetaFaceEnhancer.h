// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "MetaFaceTypes.h"
#include "YnnkVoiceLipsyncData.h"
#include "UObject/SoftObjectPtr.h"
#include "Modules/ModuleManager.h"
#include "Runtime/Launch/Resources/Version.h"

class UNeuralProcessWrapper;

/**
* YnnkMetaFaceEnhancer module
*/
class YNNKMETAFACEENHANCER_API FYnnkMetaFaceEnhancerModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Load PyTorch models for lip - sync and facial animation */
	void InitializeTorchModels(UObject* Parent = nullptr);

	/** Get pointer to NeuralProcessWrapper */
#if ENGINE_MAJOR_VERSION > 4
	TObjectPtr<UNeuralProcessWrapper> GetNeuralProcessor();
#else
	UNeuralProcessWrapper* GetNeuralProcessor();
#endif

	/** Is model valid */
	bool IsEmotionsModelReady() const;
	/** Is model valid */
	bool IsLipsyncModelReady() const;

	bool ProcessPhonemesData(const TArray<FPhonemeTextData>& PhonemesData, bool bUseLipsyncModel, TMap<FName, TArray<float>>& OutData);
	bool ProcessPhonemesData2(const TArray<FPhonemeTextData>& PhonemesData, bool bUseLipsyncModel, TMap<FName, TArray<float>>& OutData);

private:
#if ENGINE_MAJOR_VERSION > 4
	TObjectPtr<UNeuralProcessWrapper> NeuralProcessWrapper;
#else
	UNeuralProcessWrapper* NeuralProcessWrapper;
#endif
};
