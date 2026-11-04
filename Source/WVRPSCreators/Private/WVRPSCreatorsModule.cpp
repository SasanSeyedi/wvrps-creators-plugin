// WVRPS Creators module: registers the "WVRPS Creators" nomad tab (Tools > Misc) and console commands.
// All behaviour lives in UWVRPSCreatorsSubsystem (editor subsystem, also driven from editor Python).
#include "Modules/ModuleManager.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "SWVRPSCreatorsPanel.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "WVRPSCreatorsPanelHost.h"
#include "WVRPSCreatorsSubsystem.h"

#define LOCTEXT_NAMESPACE "FWVRPSCreatorsModule"

namespace WVRPSCreatorsTab
{
	static TWeakPtr<SWidget> GLivePanel;

	TSharedPtr<SWidget> GetLivePanel() { return GLivePanel.Pin(); }
	void SetLivePanel(const TSharedPtr<SWidget>& Panel) { GLivePanel = Panel; }

	static TSharedRef<SDockTab> Spawn(const FSpawnTabArgs&)
	{
		TSharedRef<SWVRPSCreatorsPanel> Panel = SNew(SWVRPSCreatorsPanel);
		SetLivePanel(Panel);
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				Panel
			];
	}

	static FAutoConsoleCommand OpenCmd(TEXT("WVRPS.Creators.Open"), TEXT("Open the WVRPS Creators panel (optional tab index 0-3)"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (UWVRPSCreatorsSubsystem* S = UWVRPSCreatorsSubsystem::Get())
			{
				S->OpenPanel(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : -1);
			}
		}));

	static FAutoConsoleCommand CloseCmd(TEXT("WVRPS.Creators.Close"), TEXT("Close the WVRPS Creators panel"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (UWVRPSCreatorsSubsystem* S = UWVRPSCreatorsSubsystem::Get())
			{
				S->ClosePanel();
			}
		}));

	static FAutoConsoleCommand ShotCmd(TEXT("WVRPS.Creators.Screenshot"), TEXT("Capture the WVRPS Creators panel to a PNG path"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (UWVRPSCreatorsSubsystem* S = UWVRPSCreatorsSubsystem::Get())
			{
				S->CapturePanelScreenshot(Args.Num() > 0 ? Args[0] : FPaths::ProjectSavedDir() / TEXT("WVRPSCreators_panel.png"));
			}
		}));
}

class FWVRPSCreatorsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(WVRPSCreatorsTab::TabName, FOnSpawnTab::CreateStatic(&WVRPSCreatorsTab::Spawn))
			.SetDisplayName(LOCTEXT("TabTitle", "WVRPS Creators"))
			.SetTooltipText(LOCTEXT("TabTooltip", "Pair with wvrps.com, browse the WVRPS creators pack and import assets through Interchange"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.SkeletalMesh"))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory());
		bRegistered = true;
	}

	virtual void ShutdownModule() override
	{
		if (bRegistered && FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(WVRPSCreatorsTab::TabName);
		}
		bRegistered = false;
	}

private:
	bool bRegistered = false;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWVRPSCreatorsModule, WVRPSCreators);
