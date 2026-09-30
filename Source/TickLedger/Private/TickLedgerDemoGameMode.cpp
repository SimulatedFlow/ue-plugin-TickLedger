// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "TickLedgerDemoGameMode.h"

#include "GameFramework/SpectatorPawn.h"

#include "TickLedgerHUD.h"

ATickLedgerDemoGameMode::ATickLedgerDemoGameMode()
{
	HUDClass = ATickLedgerHUD::StaticClass();

	// KEIN STANDARD-PAWN. `ADefaultPawn` bringt eine sichtbare Kugel mit, und die steht im
	// Ursprung — also genau in der Bildmitte zwischen den Stationen. Im ersten Bildersatz vom
	// 30.09.2026 lag sie zwischen Station 2 und 3 und sah aus, als gehoere sie dazu.
	//
	// `ASpectatorPawn` statt `nullptr`: der Spielercontroller bleibt, und nur er traegt das HUD.
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}
