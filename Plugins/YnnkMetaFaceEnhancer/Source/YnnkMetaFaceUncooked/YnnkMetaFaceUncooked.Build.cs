// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
    public class YnnkMetaFaceUncooked : ModuleRules
    {
        public YnnkMetaFaceUncooked(ReadOnlyTargetRules Target) : base(Target)
        {
            PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
            PrivatePCHHeaderFile = "Public/YnnkMetaFaceUncooked.h";

            PublicDependencyModuleNames.AddRange(
                new string[] {
                    "Engine"
                }
            );

            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
                    "SlateCore",
                    "Slate",
                    "CoreUObject",
                    "UnrealEd",
                    "Settings",
					"AnimGraphRuntime",
					"AnimGraph",
					"BlueprintGraph",
                    "YnnkVoiceLipsync",
                    "YnnkVoiceLipsyncUncooked",
                    "YnnkMetaFaceEnhancer",
                    "AnimationBlueprintLibrary",
                    "AnimationModifiers",
                    "ContentBrowser",
                    "AssetTools",
                    "AssetRegistry",
                    "LiveLinkAnimationCore",
                    "ToolMenus",
                    "PropertyEditor"
                }
            );
        }
    }
}