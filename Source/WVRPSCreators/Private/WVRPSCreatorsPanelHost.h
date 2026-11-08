// Shared between the module (spawns the tab) and the subsystem (opens / screenshots it).
#pragma once

#include "CoreMinimal.h"

class SWidget;

namespace WVRPSCreatorsTab
{
	inline const FName TabName(TEXT("WVRPSCreators"));

	/** The live panel widget, or null when the tab is closed. Set by the tab spawner. */
	TSharedPtr<SWidget> GetLivePanel();
	void SetLivePanel(const TSharedPtr<SWidget>& Panel);
}
