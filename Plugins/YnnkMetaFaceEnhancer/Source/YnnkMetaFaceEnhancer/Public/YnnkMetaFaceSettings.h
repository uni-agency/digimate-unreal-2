// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "MetaFaceTypes.h"
#include "YnnkMetaFaceSettings.generated.h"

/**
* Settings for Ynnk MetaFace Generator
*/
UCLASS(config = Engine, defaultconfig)
class YNNKMETAFACEENHANCER_API UYnnkMetaFaceSettings : public UObject
{
	GENERATED_BODY()

public:
	UYnnkMetaFaceSettings();

	/** Intensity of facial animation played by controller */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "General", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float EmotionsIntensity;

	/**
	* Intensity of pre-defined visemes (applid with LipsyncNeuralIntensity < 1)
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "General", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float VisemeApplyAlpha;

	/**
	* Alpha to blend form neural net to defined visemes.
	* Note: blending to Ynnk Voice LipSync in animation blueprint is preferreable.
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "General", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float LipsyncNeuralIntensity;

	/**
	* Increase to make lip-sync animation more smooth (and lose details)
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "General", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float LipsyncSmoothness;

	/**
	* Increase to make facial animation/emotions more smooth (and lose details)
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "General", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float FacialAnimationSmoothness;

	/**
	* Pre-defined facial poses for visemes. Don't modify this variable.
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "General")
	TMap<EYnnkViseme, FMetaFacePose> LipsyncVisemesPreset;

	/**
	* Keep Frown curves not less then smile curves
	* (Enable this checkbox for CC3/CC4 character)
	*/		
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "General")
	bool bBalanceSmileFrownCurves;
};