// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

#include "YnnkMetaFaceEnhancer.h"
#include "SimpleTorchModule.h"
#include "MetaFaceTypes.h"
#include "YnnkVoiceLipsyncData.h"
#include "Interfaces/IPluginManager.h"
#include "NeuralProcessWrapper.h"
#include "Engine/Engine.h"
#include "Runtime/Launch/Resources/Version.h"

#define LOCTEXT_NAMESPACE "FYnnkMetaFaceEnhancerModule"

void FYnnkMetaFaceEnhancerModule::StartupModule()
{
	//NeuralProcessWrapper = nullptr;
	NeuralProcessWrapper = NewObject<UNeuralProcessWrapper>();
	if (IsValid(NeuralProcessWrapper))
	{
		NeuralProcessWrapper->AddToRoot();
		NeuralProcessWrapper->Initialize();
	}
}

void FYnnkMetaFaceEnhancerModule::ShutdownModule()
{
	if (IsValid(NeuralProcessWrapper))
	{
		if (NeuralProcessWrapper->IsRooted())
		{
			NeuralProcessWrapper->RemoveFromRoot();
		}
		__uev_destory_object(NeuralProcessWrapper);

		NeuralProcessWrapper = nullptr;
	}
}

void FYnnkMetaFaceEnhancerModule::InitializeTorchModels(UObject* Parent)
{	
	/*
	if (IsValid(NeuralProcessWrapper))
	{
		if (NeuralProcessWrapper->IsRooted())
		{
			NeuralProcessWrapper->RemoveFromRoot();
		}
		__uev_destory_object(NeuralProcessWrapper);
	}
	
	if (IsValid(Parent) && Parent->IsA(UNeuralProcessWrapper::StaticClass()))
	{
		NeuralProcessWrapper = Cast<UNeuralProcessWrapper>(Parent);
	}
	else
	{
		NeuralProcessWrapper = NewObject<UNeuralProcessWrapper>();
	}

	if (NeuralProcessWrapper)
	{
		NeuralProcessWrapper->Initialize();
	}
	*/
}

#if ENGINE_MAJOR_VERSION > 4
TObjectPtr<UNeuralProcessWrapper> FYnnkMetaFaceEnhancerModule::GetNeuralProcessor()
#else
UNeuralProcessWrapper* FYnnkMetaFaceEnhancerModule::GetNeuralProcessor()
#endif
{
	if (!IsValid(NeuralProcessWrapper) || !NeuralProcessWrapper->IsValid())
	{
		//InitializeTorchModels(nullptr);
		if (!IsValid(NeuralProcessWrapper))
		{
			NeuralProcessWrapper = NewObject<UNeuralProcessWrapper>();
		}
		if (IsValid(NeuralProcessWrapper))
		{
			NeuralProcessWrapper->AddToRoot();
			NeuralProcessWrapper->Initialize();
		}
	}

	return NeuralProcessWrapper;
}

bool FYnnkMetaFaceEnhancerModule::IsEmotionsModelReady() const
{
	return NeuralProcessWrapper && NeuralProcessWrapper->IsEmotionsModelReady();
}

bool FYnnkMetaFaceEnhancerModule::IsLipsyncModelReady() const
{
	return NeuralProcessWrapper && NeuralProcessWrapper->IsLipsyncModelReady();
}

bool FYnnkMetaFaceEnhancerModule::ProcessPhonemesData(const TArray<FPhonemeTextData>& PhonemesData, bool bUseLipsyncModel, TMap<FName, TArray<float>>& OutData)
{
	if (NeuralProcessWrapper && NeuralProcessWrapper->IsValid())
	{
		return NeuralProcessWrapper->ProcessPhonemesData(PhonemesData, bUseLipsyncModel, OutData);
	}
	else
	{
		return false;
	}
}

bool FYnnkMetaFaceEnhancerModule::ProcessPhonemesData2(const TArray<FPhonemeTextData>& PhonemesData, bool bUseLipsyncModel, TMap<FName, TArray<float>>& OutData)
{
	if (NeuralProcessWrapper && NeuralProcessWrapper->IsValid())
	{
		return NeuralProcessWrapper->ProcessPhonemesData2(PhonemesData, bUseLipsyncModel, OutData);
	}
	else
	{
		return false;
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FYnnkMetaFaceEnhancerModule, YnnkMetaFaceEnhancer)