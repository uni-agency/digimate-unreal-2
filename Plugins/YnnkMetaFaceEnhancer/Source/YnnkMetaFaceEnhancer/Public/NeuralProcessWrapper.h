// (c) Yuri N. K. 2022. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "SimpleTorchModule.h"
#include "MetaFaceTypes.h"
#include "YnnkVoiceLipsyncData.h"
#include "HAL/CriticalSection.h"
#include "NeuralProcessWrapper.generated.h"

/**
 * Helper class to build facial animation from phonemes data
 */
UCLASS(BlueprintType)
class YNNKMETAFACEENHANCER_API UNeuralProcessWrapper : public UObject
{
	GENERATED_BODY()

public:
	UNeuralProcessWrapper();

	UFUNCTION()
	void Initialize();

	UFUNCTION()
	bool IsValid() const;

	UFUNCTION()
	bool IsLipsyncModelReady() const;

	UFUNCTION()
	bool IsEmotionsModelReady() const;
	
	bool ProcessPhonemesData(const TArray<FPhonemeTextData>& PhonemesData, bool bUseLipsyncModel, TMap<FName, TArray<float>>& OutData);
	bool ProcessPhonemesData2(const TArray<FPhonemeTextData>& PhonemesData, bool bUseLipsyncModel, TMap<FName, TArray<float>>& OutData);

	void InterruptAll();

protected:

	// Names of curves
	UPROPERTY()
	TArray<FName> NN_EmotionsOutCurves;
	UPROPERTY()
	TArray<FName> NN_LipsyncOutCurves;

	static FCriticalSection NeuralWrapMutex1;
	static FCriticalSection NeuralWrapMutex2;

	// Facial animation: in0out tensors and model
	UPROPERTY()
	FSimpleTorchTensor EmotionsInData;
	UPROPERTY()
	FSimpleTorchTensor EmotionsOutData;
	UPROPERTY()
	USimpleTorchModule* EmotionsNeuralModel;
	UPROPERTY()
	bool bEmotionsModelReady;

	// Lip-Sync: in0out tensors and model
	UPROPERTY()
	FSimpleTorchTensor LipsyncInData;
	UPROPERTY()
	FSimpleTorchTensor LipsyncOutData;
	UPROPERTY()
	USimpleTorchModule* LipsyncNeuralModel;
	UPROPERTY()
	bool bLipsyncModelReady;

	bool bProcessingModel1 = false;
	bool bProcessingModel2 = false;

	FString GetResourcesPath() const;
};
