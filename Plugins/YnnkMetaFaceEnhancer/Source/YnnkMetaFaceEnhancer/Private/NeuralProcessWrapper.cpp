// (c) Yuri N. K. 2022. All rights reserved.
// ykasczc@gmail.com

#include "NeuralProcessWrapper.h"
#include "MetaFaceFunctionLibrary.h"
#include "MetaFaceTypes.h"
#include "YnnkVoiceLipsyncData.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/CriticalSection.h"
#include "Containers/StringConv.h"
#include "Misc/Paths.h"

// Critical session for ProcessPhonemesData
FCriticalSection UNeuralProcessWrapper::NeuralWrapMutex1;
FCriticalSection UNeuralProcessWrapper::NeuralWrapMutex2;

UNeuralProcessWrapper::UNeuralProcessWrapper()
	: EmotionsNeuralModel(nullptr)
	, bEmotionsModelReady(false)
	, LipsyncNeuralModel(nullptr)
	, bLipsyncModelReady(false)
{
	UMFFunctionLibrary::GetMetaFaceCurvesSet(NN_EmotionsOutCurves, false);
	UMFFunctionLibrary::GetMetaFaceCurvesSet(NN_LipsyncOutCurves, true);
}

void UNeuralProcessWrapper::Initialize()
{
	bEmotionsModelReady = false;
	bLipsyncModelReady = false;

	FString ResourcesPath = GetResourcesPath();

	FString FileName = ResourcesPath / TEXT("ynnkemotions_en.tmod");
	if (!FPaths::FileExists(FileName))
	{
		FileName = ResourcesPath / TEXT("ynnkemotions_en_editor.tmod");
	}

	EmotionsNeuralModel = USimpleTorchModule::CreateSimpleTorchModule(this);
	if (EmotionsNeuralModel)
	{
		if (FPaths::FileExists(FileName) && EmotionsNeuralModel->LoadTorchScriptModel(FileName))
		{
			EmotionsInData.Create({ 1 });
			EmotionsOutData.Create({ 1, NN_EmotionsOutCurves.Num() });
			bEmotionsModelReady = true;
		}
		else
		{
			UE_LOG(LogMetaFace, Warning, TEXT("Unable to load torch jit model from file (%s)"), *FileName);
		}
	}
	else
	{
		UE_LOG(LogMetaFace, Warning, TEXT("Unable to create torch jit model wrapper"));
	}

	// new: lip-sync model

	FileName = ResourcesPath / TEXT("ynnklipsync.tmod");
	if (!FPaths::FileExists(FileName))
	{
		FileName = ResourcesPath / TEXT("ynnklipsync_editor.tmod");
	}
	LipsyncNeuralModel = USimpleTorchModule::CreateSimpleTorchModule(this);
	if (LipsyncNeuralModel)
	{
		if (FPaths::FileExists(FileName) && LipsyncNeuralModel->LoadTorchScriptModel(FileName))
		{
			LipsyncInData.Create({ 1 });
			LipsyncOutData.Create({ 1, NN_LipsyncOutCurves.Num() });
			bLipsyncModelReady = true;
		}
		else
		{
			UE_LOG(LogMetaFace, Warning, TEXT("Unable to load torch jit model from file (%s)"), *FileName);
		}
	}
	else
	{
		UE_LOG(LogMetaFace, Warning, TEXT("Unable to create torch jit model wrapper"));
	}
}

bool UNeuralProcessWrapper::IsValid() const
{
	return bEmotionsModelReady && bLipsyncModelReady;
}

bool UNeuralProcessWrapper::IsLipsyncModelReady() const
{
	return bLipsyncModelReady;
}

bool UNeuralProcessWrapper::IsEmotionsModelReady() const
{
	return bEmotionsModelReady;
}

void UNeuralProcessWrapper::InterruptAll()
{
	bProcessingModel1 = false;
	bProcessingModel2 = false;
}

bool UNeuralProcessWrapper::ProcessPhonemesData(const TArray<FPhonemeTextData>& PhonemesData, bool bUseLipsyncModel, TMap<FName, TArray<float>>& OutData)
{
	OutData.Empty();

	// Check NN model
	if ((bUseLipsyncModel && !bLipsyncModelReady) || (!bUseLipsyncModel && !bEmotionsModelReady))
	{
		UE_LOG(LogMetaFace, Warning, TEXT("ProcessPhonemesData: model isn't ready"));
		return false;
	}
	if (PhonemesData.Num() == 0)
	{
		UE_LOG(LogMetaFace, Warning, TEXT("PhonemesData is empty. Enable it in Project Settings > [Plugins] Ynnk Lip-sync > Generate Phonemes Data"));
		return false;
	}

	bProcessingModel1 = true;

	const auto& CurvesSet = bUseLipsyncModel
		? NN_LipsyncOutCurves
		: NN_EmotionsOutCurves;
	auto& nnModel = bUseLipsyncModel
		? LipsyncNeuralModel
		: EmotionsNeuralModel;
	/*
	auto& InTensor = bUseLipsyncModel
		? LipsyncInData
		: EmotionsInData;
	auto& OutTensor = bUseLipsyncModel
		? LipsyncOutData
		: EmotionsOutData;
	*/
	FSimpleTorchTensor InTensor, OutTensor;
	InTensor.Create({ 1 });
	OutTensor.Create({ 1, CurvesSet.Num() });

	FScopeLock Lock(&NeuralWrapMutex1);

	const ANSICHAR cFirst = 'a', cLast = 'z';
	const ANSICHAR c0 = '0', c9 = '9';

	// don't mirror curves
	bool bMirrorLeftToRight = false;

	// create curves
	for (const auto& ColumnName : CurvesSet)
	{
		FString szCurveName = ColumnName.ToString();
		OutData.Add(ColumnName);

		// mirror right curves
		if (bMirrorLeftToRight && szCurveName.Left(5) == TEXT("Mouth") && szCurveName.Right(4) == TEXT("Left"))
		{
			szCurveName = szCurveName.LeftChop(4) + TEXT("Right");
			OutData.Add(*szCurveName);
		}
	}

	// Compute floats
	int32 DebugCounter = 0;
	for (const auto& Phoneme : PhonemesData)
	{
		DebugCounter++;

		if (Phoneme.Symbol.Len() != 1)
		{
			UE_LOG(LogMetaFace, Log, TEXT("ProcessPhonemesData: invalid symbol \"%s\""), *Phoneme.Symbol);
			bProcessingModel1 = false;
			return false;
		}

		ANSICHAR ansi[16];
		ansi[0] = StringCast<ANSICHAR>(*Phoneme.Symbol).Get()[0];
		if (!(ansi[0] >= cFirst && ansi[0] <= cLast) && !(ansi[0] >= c0 && ansi[0] <= c9))
		{
			UE_LOG(LogMetaFace, Log, TEXT("ProcessPhonemesData can't read phoneme data: \"%s\""), *Phoneme.Symbol);
			//bProcessingModel1 = false;
			//return false;
			continue;
		}

		int32 SymbolNum = (ansi[0] - cFirst) * 2 + 1;
		bool bNewWordStarted = Phoneme.bWordStart;
		if (bNewWordStarted) { SymbolNum++; }
		InTensor[0] = (float)SymbolNum;

		// single-symbol evaluation
		if (!nnModel)
		{
			UE_LOG(LogMetaFace, Log, TEXT("Invalid nnModel"));
			bProcessingModel1 = false;
			return false;
		}

		//FScopeLock Lock(&CritSectionObject);

		bool bResult = nnModel->ExecuteModelMethod(TEXT("eval_compute"), InTensor, OutTensor);
		if (!bResult || !bProcessingModel1)
		{
			bProcessingModel1 = false;
			return false;
		}

		check(OutTensor.GetDataSize() == CurvesSet.Num());

		for (int32 n = 0; n < CurvesSet.Num(); n++)
		{
			const FName& CurveName = CurvesSet[n];

			TArray<int32> Dims = OutTensor.GetDimensions();
			if (Dims.Num() < 2) Dims.SetNum(2);

			FSimpleTorchTensor t;
			t.CreateAsChild(&OutTensor, { 0, n });
			// get NN value
			float val = FMath::Clamp(t.Item(), -1.f, 1.f);
			OutData[CurveName].Add(val);

			// mirror right curves
			FString szCurveName = CurveName.ToString();
			if (bMirrorLeftToRight && szCurveName.Left(5) == TEXT("Mouth") && szCurveName.Right(4) == TEXT("Left"))
			{
				szCurveName = szCurveName.LeftChop(4) + TEXT("Right");
				OutData[*szCurveName].Add(val);
			}
		}
	}

	bProcessingModel1 = false;
	return true;
}

bool UNeuralProcessWrapper::ProcessPhonemesData2(const TArray<FPhonemeTextData>& PhonemesData, bool bUseLipsyncModel /* false */, TMap<FName, TArray<float>>& OutData)
{
	OutData.Empty();

	// Check NN model
	if ((bUseLipsyncModel && !bLipsyncModelReady) || (!bUseLipsyncModel && !bEmotionsModelReady))
	{
		UE_LOG(LogMetaFace, Warning, TEXT("ProcessPhonemesData: model isn't ready"));
		return false;
	}
	if (PhonemesData.Num() == 0)
	{
		UE_LOG(LogMetaFace, Warning, TEXT("PhonemesData is empty. Enable it in Project Settings > [Plugins] Ynnk Lip-sync > Generate Phonemes Data"));
		return false;
	}

	bProcessingModel2 = true;

	const auto& CurvesSet = NN_EmotionsOutCurves;
	auto& nnModel = EmotionsNeuralModel;
	/*
	auto& InTensor = EmotionsInData;
	auto& OutTensor = EmotionsOutData;
	*/
	FSimpleTorchTensor InTensor, OutTensor;
	InTensor.Create({ 1 });
	OutTensor.Create({ 1, CurvesSet.Num() });
	
	FScopeLock Lock(&NeuralWrapMutex2);

	const ANSICHAR cFirst = 'a', cLast = 'z';
	const ANSICHAR c0 = '0', c9 = '9';

	// don't mirror curves
	bool bMirrorLeftToRight = false;

	// create curves
	for (const auto& ColumnName : CurvesSet)
	{
		FString szCurveName = ColumnName.ToString();
		OutData.Add(ColumnName);

		// mirror right curves
		if (bMirrorLeftToRight && szCurveName.Left(5) == TEXT("Mouth") && szCurveName.Right(4) == TEXT("Left"))
		{
			szCurveName = szCurveName.LeftChop(4) + TEXT("Right");
			OutData.Add(*szCurveName);
		}
	}

	// Compute floats
	int32 DebugCounter = 0.f;
	for (const auto& Phoneme : PhonemesData)
	{
		DebugCounter++;

		if (Phoneme.Symbol.Len() != 1)
		{
			UE_LOG(LogMetaFace, Log, TEXT("ProcessPhonemesData2: invalid symbol \"%s\""), *Phoneme.Symbol);
			bProcessingModel2 = false;
			return false;
		}
		ANSICHAR ansi[16];
		ansi[0] = StringCast<ANSICHAR>(*Phoneme.Symbol).Get()[0];
		if (!(ansi[0] >= cFirst && ansi[0] <= cLast) && !(ansi[0] >= c0 && ansi[0] <= c9))
		{
			UE_LOG(LogMetaFace, Log, TEXT("ProcessPhonemesData can't read phoneme data: \"%s\""), *Phoneme.Symbol);
			//bProcessingModel2 = false;
			//return false;
			continue;
		}

		int32 SymbolNum = (ansi[0] - cFirst) * 2 + 1;
		bool bNewWordStarted = Phoneme.bWordStart;
		if (bNewWordStarted) { SymbolNum++; }
		InTensor[0] = (float)SymbolNum;

		// single-symbol evaluation
		if (!nnModel)
		{
			UE_LOG(LogMetaFace, Log, TEXT("Invalid nnModel"));
			bProcessingModel2 = false;
			return false;
		}

		bool bResult = nnModel->ExecuteModelMethod(TEXT("eval_compute"), InTensor, OutTensor);
		if (!bResult || !bProcessingModel2)
		{
			bProcessingModel2 = false;
			return false;
		}

		check(OutTensor.GetDataSize() == CurvesSet.Num());

		for (int32 n = 0; n < CurvesSet.Num(); n++)
		{
			const FName& CurveName = CurvesSet[n];

			TArray<int32> Dims = OutTensor.GetDimensions();
			if (Dims.Num() < 2) Dims.SetNum(2);

			FSimpleTorchTensor t;
			t.CreateAsChild(&OutTensor, { 0, n });
			// get NN value
			float val = FMath::Clamp(t.Item(), -1.f, 1.f);
			OutData[CurveName].Add(val);

			// mirror right curves
			FString szCurveName = CurveName.ToString();
			if (bMirrorLeftToRight && szCurveName.Left(5) == TEXT("Mouth") && szCurveName.Right(4) == TEXT("Left"))
			{
				szCurveName = szCurveName.LeftChop(4) + TEXT("Right");
				OutData[*szCurveName].Add(val);
			}
		}
	}

	bProcessingModel2 = false;
	return true;
}

FString UNeuralProcessWrapper::GetResourcesPath() const
{
	FString ResourcesPath;

#if WITH_EDITOR
	auto ThisPlugin = IPluginManager::Get().FindPlugin(TEXT("YnnkMetaFaceEnhancer"));
	if (ThisPlugin.IsValid())
	{
		ResourcesPath = FPaths::ConvertRelativePathToFull(ThisPlugin->GetBaseDir()) / TEXT("Resources");
	}
	else
	{
		ResourcesPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) / TEXT("Resources");
	}
#else
	ResourcesPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) / TEXT("Plugins") / TEXT("YnnkMetaFaceEnhancer") / TEXT("Resources");
#endif
	return ResourcesPath;
}