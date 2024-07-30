// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

#include "AsyncAnimBuilder.h"
#include "Async/Future.h"
#include "Async/Async.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "YnnkMetaFaceEnhancer.h"
#include "YnnkVoiceLipsyncData.h"
#include "YnnkMetaFaceController.h"
#include "Animation/PoseAsset.h"
#include "MetaFaceFunctionLibrary.h"
#include "MetaFaceTypes.h"
#include "YnnkMetaFaceSettings.h"
#include "NeuralProcessWrapper.h"
#include "HAL/CriticalSection.h"
#include "Runtime/Launch/Resources/Version.h"

FCriticalSection UAsyncAnimBuilder::StartAsyncMutex;

UAsyncAnimBuilder::UAsyncAnimBuilder()
	: LipsyncData(nullptr)
	, bSaveGeneratedAnimationInLipsyncData(false)
	, bGenerateLipsync(false)
	, bGenerateFacialAnimation(false)
	, bIsWorking(false)
	, bExecutionInterrupted(false)
	, bLipsyncReady(false)
	, bFacialAnimationReady(false)
{
	NeuralProcessor = nullptr;
}

UAsyncAnimBuilder* UAsyncAnimBuilder::CreateAsyncAnimBuilder(UYnnkVoiceLipsyncData* InLipsyncData, bool bLipsync, bool bFacialAnimation, const FAsyncFacialAnimationResult& InCallbackEvent)
{
	UAsyncAnimBuilder* Builder = CreateAsyncAnimBuilder(InLipsyncData, bLipsync, bFacialAnimation);
	if (Builder)
	{
		Builder->CallbackEvent = InCallbackEvent;
		Builder->bSaveGeneratedAnimationInLipsyncData = false;
	}
	return Builder;
}

UAsyncAnimBuilder* UAsyncAnimBuilder::CreateAsyncAnimBuilder(UYnnkVoiceLipsyncData* InLipsyncData, bool bLipsync, bool bFacialAnimation)
{
	UAsyncAnimBuilder* Builder = /*InLipsyncData
		? NewObject<UAsyncAnimBuilder>(InLipsyncData)
		: */NewObject<UAsyncAnimBuilder>(UAsyncAnimBuilder::StaticClass());

	if (Builder)
	{
		Builder->LipsyncData = InLipsyncData;
		Builder->bGenerateLipsync = bLipsync;
		Builder->bGenerateFacialAnimation = bFacialAnimation;

		auto Settings = GetDefault<UYnnkMetaFaceSettings>();
		Builder->EmotionsIntensity = Settings->EmotionsIntensity;
		Builder->VisemeApplyAlpha = Settings->VisemeApplyAlpha;
		Builder->LipsyncNeuralIntensity = Settings->LipsyncNeuralIntensity;
		Builder->LipsyncSmoothness = Settings->LipsyncSmoothness;
		Builder->FacialAnimationSmoothness = Settings->FacialAnimationSmoothness;
		Builder->bBalanceSmileFrownCurves = Settings->bBalanceSmileFrownCurves;
		Builder->bLipSyncToSkeletonCurves = false;
		Builder->bFacialAnimationToSkeletonCurves = false;		
	}
	return Builder;
}

void UAsyncAnimBuilder::StartAsQueue(TArray<UYnnkVoiceLipsyncData*>& InOutLipsyncDataAssets, const FAsyncMetaFaceQueueResult& InProcessResultEvent)
{
	bSaveGeneratedAnimationInLipsyncData = true;
	ProcessingQueue = InOutLipsyncDataAssets;
	ProcessResultEvent = InProcessResultEvent;
	QueueSize = ProcessingQueue.Num() + 1;

	if (!LipsyncData)
	{
		QueueSize--;

		if (ProcessingQueue.Num() == 0)
		{
			__uev_destory_object(this);
			return;
		}
		LipsyncData = ProcessingQueue.Pop();
	}

	Start();
}

void UAsyncAnimBuilder::OverrideSettings(UYnnkMetaFaceController* Controller)
{
	if (Controller)
	{
		EmotionsIntensity = Controller->EmotionsIntensity;
		VisemeApplyAlpha = Controller->VisemeApplyAlpha;
		LipsyncNeuralIntensity = Controller->LipsyncNeuralIntensity;
		LipsyncSmoothness = Controller->LipsyncSmoothness;
		FacialAnimationSmoothness = Controller->FacialAnimationSmoothness;
		ArKitCurvesPoseAsset = Controller->ArKitCurvesPoseAsset;
		bLipSyncToSkeletonCurves = false; //Controller->bLipSyncToSkeletonCurves;
		bFacialAnimationToSkeletonCurves = false; // Controller->bFacialAnimationToSkeletonCurves;
		bBalanceSmileFrownCurves = Controller->bBalanceSmileFrownCurves;
	}
}

void UAsyncAnimBuilder::Start()
{
	StartAsyncMutex.Lock();

	if (!LipsyncData)
	{
		FMHFacialAnimation AnimLS, AnimFA;
		CallbackEvent.ExecuteIfBound(AnimLS, AnimFA);
		return;
	}
	if (!IsValid(NeuralProcessor))
	{
		auto ModuleMFE = FModuleManager::GetModulePtr<FYnnkMetaFaceEnhancerModule>(TEXT("YnnkMetaFaceEnhancer"));
		NeuralProcessor = ModuleMFE->GetNeuralProcessor();
	}

	bIsWorking = true;

	// Start import thread
	WorkingThreadFA = Async(EAsyncExecution::Thread, [this]()
	{
		FString OutData;
		bool bResult = false;
		float TimeOffset = 0.f;
		auto ModuleMFE = FModuleManager::GetModulePtr<FYnnkMetaFaceEnhancerModule>(TEXT("YnnkMetaFaceEnhancer"));

		if (ModuleMFE)
		{
			RawAnimDataMap GeneratedData;

			OutLipsyncData.Empty();
			OutFacialAnimationData.Empty();

			// Lip-sync
			if (bGenerateLipsync && !bExecutionInterrupted)
			{
				if (NeuralProcessor->ProcessPhonemesData(LipsyncData->PhonemesData, true, GeneratedData))
				{
					if (!bExecutionInterrupted)
					{
						UMFFunctionLibrary::RawDataToLipsync(LipsyncData, GeneratedData, OutLipsyncData, FMetaFaceGenerationSettings(this));

						const FName FrownL = TEXT("MouthFrownLeft");
						const FName FrownR = TEXT("MouthFrownRight");
						const FName SmileL = TEXT("MouthSmileLeft");
						const FName SmileR = TEXT("MouthSmileRight");

						if (bBalanceSmileFrownCurves
							&& OutLipsyncData.Contains(FrownL) && OutLipsyncData.Contains(SmileL)
							&& OutLipsyncData.Contains(FrownR) && OutLipsyncData.Contains(SmileR))
						{
							for (int32 i = 0; i < OutLipsyncData[FrownL].Values.Num(); i++)
							{
								if (OutLipsyncData[FrownL].Values[i].Value < OutLipsyncData[SmileL].Values[i].Value)
								{
									float Mean = (OutLipsyncData[FrownL].Values[i].Value + OutLipsyncData[SmileL].Values[i].Value) * 0.5f;
									OutLipsyncData[FrownL].Values[i].Value = OutLipsyncData[SmileL].Values[i].Value = Mean;
								}

								if (OutLipsyncData[FrownR].Values[i].Value < OutLipsyncData[SmileR].Values[i].Value)
								{
									float Mean = (OutLipsyncData[FrownR].Values[i].Value + OutLipsyncData[SmileR].Values[i].Value) * 0.5f;
									OutLipsyncData[FrownR].Values[i].Value = OutLipsyncData[SmileR].Values[i].Value = Mean;
								}
							}
						}

						if (bLipSyncToSkeletonCurves)
						{
							UMFFunctionLibrary::ConvertFacialAnimCurves(OutLipsyncData, ArKitCurvesPoseAsset);
						}
					}
				}
				else
				{
					OutLipsyncData.Empty();
					OutData = TEXT("ERROR: can't process data");
				}
			}

			// Facial Animation
			if (bGenerateFacialAnimation && !bExecutionInterrupted)
			{
				GeneratedData.Empty();
				if (NeuralProcessor->ProcessPhonemesData2(LipsyncData->PhonemesData, false, GeneratedData))
				{
					if (!bExecutionInterrupted)
					{
						UMFFunctionLibrary::RawDataToFacialAnimation(LipsyncData, GeneratedData, OutFacialAnimationData, FMetaFaceGenerationSettings(this));
						// @TODO: an obvious problem with smile-frown!

						if (bBalanceSmileFrownCurves)
						{
							/*
							for (auto& Value : OutFacialAnimationData[TEXT("MouthSmileLeft")].Values)
							{
								Value.Value *= 0.5f;
							}
							OutFacialAnimationData.Add(TEXT("MouthFrownLeft"), OutFacialAnimationData[TEXT("MouthSmileLeft")]);

							for (auto& Value : OutFacialAnimationData[TEXT("MouthSmileRight")].Values)
							{
								Value.Value *= 0.5f;
							}
							OutFacialAnimationData.Add(TEXT("MouthFrownRight"), OutFacialAnimationData[TEXT("MouthSmileRight")]);
							*/
							OutFacialAnimationData.Remove(TEXT("MouthSmileLeft"));
							OutFacialAnimationData.Remove(TEXT("MouthSmileRight"));
						}

						if (bFacialAnimationToSkeletonCurves)
						{
							UMFFunctionLibrary::ConvertFacialAnimCurves(OutFacialAnimationData, ArKitCurvesPoseAsset);
						}
					}
				}
				else
				{
					OutFacialAnimationData.Empty();
					OutData = TEXT("ERROR: can't process data");
				}
			}
		}
		else
		{
			OutData = TEXT("ERROR: can't get pointer to module");
		}


		// Main job done here
		if (!bExecutionInterrupted)
		{
			AsyncTask(ENamedThreads::GameThread, [this]()
			{
				StartAsyncMutex.Unlock();
				OnAnimationReady();
			});
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this]()
			{
				StartAsyncMutex.Unlock();
				OnAnimationFailed();
			});
		}
	});
}

bool UAsyncAnimBuilder::IsWorking() const
{
	return bIsWorking;
}

void UAsyncAnimBuilder::Stop()
{
	if (bIsWorking)
	{
		auto ModuleMFE = FModuleManager::GetModulePtr<FYnnkMetaFaceEnhancerModule>(TEXT("YnnkMetaFaceEnhancer"));
		if (ModuleMFE)
		{
			NeuralProcessor = ModuleMFE->GetNeuralProcessor();
			if (IsValid(NeuralProcessor))
			{
				NeuralProcessor->InterruptAll();
			}
		}

		bExecutionInterrupted = true;
		bIsWorking = false;
	}
}

void UAsyncAnimBuilder::OnAnimationFailed()
{
	FMHFacialAnimation AnimLS, AnimFA;
	CallbackEvent.ExecuteIfBound(AnimLS, AnimFA);
}

void UAsyncAnimBuilder::OnAnimationReady()
{
	bLipsyncReady = bGenerateLipsync;
	bFacialAnimationReady = bGenerateFacialAnimation;

	// batch update?
	if (bSaveGeneratedAnimationInLipsyncData)
	{
		ProcessResultEvent.ExecuteIfBound(LipsyncData, QueueSize - ProcessingQueue.Num(), QueueSize);

		if (ProcessingQueue.Num() > 0)
		{
			LipsyncData = ProcessingQueue.Pop();
			Start();
			return;
		}
		else
		{
			bIsWorking = false;
		}
	}
	// generate animation for external usage
	else
	{
		FMHFacialAnimation AnimLS, AnimFA;

		if (!bExecutionInterrupted)
		{
			if (bGenerateLipsync)
			{
				AnimLS.Initialize(OutLipsyncData, false);
			}
			if (bGenerateFacialAnimation)
			{
				AnimFA.Initialize(OutFacialAnimationData, true, 1.f, 0.49f);
			}

			CallbackEvent.ExecuteIfBound(AnimLS, AnimFA);
		}
		else
		{
			CallbackEvent.ExecuteIfBound(AnimLS, AnimFA);
		}
		bIsWorking = false;
	}
}
