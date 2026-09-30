// Copyright 2026 Silvan Teufel. All Rights Reserved.

using UnrealBuildTool;

public class TickLedger : ModuleRules
{
	public TickLedger(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Wie JointLedger: alles, was dieses Werkzeug anfasst, steht in "Engine" —
		// `FTickFunction`, `AActor::PrimaryActorTick` und `UActorComponent` liegen dort.
		// Ein Abhaengigkeitseintrag, den niemand braucht, ist genauso eine Vermutung wie ein
		// fehlender; der Bau belegt das.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json",
			"JsonUtilities",
			"RenderCore"
		});
	}
}
