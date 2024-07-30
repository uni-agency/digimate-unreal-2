// (c) Yuri N. K. 2021. All rights reserved.
// ykasczc@gmail.com

using UnrealBuildTool;
using System.IO;

public class YnnkMetaFaceEnhancer : ModuleRules
{
	private string ResourcesPath
	{
		get { return Path.GetFullPath(Path.Combine(PluginDirectory, "Resources")); }
	}

	public YnnkMetaFaceEnhancer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"ApplicationCore",
				"SimplePyTorch",
				"Json",
				"Projects",
				"AnimGraphRuntime",
				"AnimationCore",
				"YnnkVoiceLipsync"
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// In editor build this would create nested directory in the plugin's directory, because $(BinaryOutputDir) = Plugins/SGVRIK/Binaries
			if (!Target.bBuildEditor)
			{
				RuntimeDependencies.Add("$(BinaryOutputDir)/../../Plugins/YnnkMetaFaceEnhancer/Resources/ynnklipsync.tmod", Path.Combine(ResourcesPath, "ynnklipsync_editor.tmod"), StagedFileType.NonUFS);
				RuntimeDependencies.Add("$(BinaryOutputDir)/../../Plugins/YnnkMetaFaceEnhancer/Resources/ynnkemotions_en.tmod", Path.Combine(ResourcesPath, "ynnkemotions_en_editor.tmod"), StagedFileType.NonUFS);
			}
		}
	}
}
