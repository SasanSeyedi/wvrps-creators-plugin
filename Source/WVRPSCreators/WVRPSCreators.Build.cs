using UnrealBuildTool;

public class WVRPSCreators : ModuleRules
{
	public WVRPSCreators(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"EditorSubsystem",        // UWVRPSCreatorsSubsystem (the panel and editor Python drive the same object)
		});

		// Stock-engine modules only (UE 5.4 / 5.5 launcher builds). Same panel set as WVRPSTools plus the
		// creator-specific four: HTTP (Creator API), InterchangeEngine/InterchangeCore (ImportAssetAsync),
		// InterchangePipelines (skeleton override on the generic assets pipeline). No third-party code.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",               // GEditor, editor subsystems
			"Json",                   // FJsonObject, TJsonReader/Writer (API bodies, catalog manifest)
			"HTTP",                   // FHttpModule - every network call is async, callbacks on the game thread
			"InterchangeEngine",      // UInterchangeManager::ImportAssetAsync, UInterchangeProjectSettings
			"InterchangeCore",        // UInterchangeSourceData, UInterchangePipelineBase
			"InterchangePipelines",   // UInterchangeGenericAssetsPipeline (CommonSkeletalMeshesAndAnimationsProperties->Skeleton)
			"Slate",                  // SDockTab, STileView, SListView, SSegmentedControl - the panel
			"SlateCore",              // brushes, style, geometry
			"WorkspaceMenuStructure", // filing the nomad tab under Tools
			"Projects",               // IPluginManager: the plugin's own version in About
			"InputCore",
			"ImageWrapper",           // PNG/JPG thumbnail decode (off the game thread)
			"ContentBrowser",         // "Show in Content Browser"
			"AssetRegistry",          // asset path -> FAssetData
			"AssetTools",             // UAssetImportTask: wav/mp3 -> USoundWave through the stock audio factory
			"ApplicationCore",        // clipboard (copy code)
		});
	}
}
