// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "TickLedgerTypes.h"

#include "TickLedgerSettings.generated.h"

/**
 * Die Einstellungen — an EINER Stelle, damit es nicht zwei Wahrheiten gibt.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "TickLedger"))
class TICKLEDGER_API UTickLedgerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UTickLedgerSettings* Get() { return GetDefault<UTickLedgerSettings>(); }

	/** Wird der Kasten gezeichnet? */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "TickLedger")
	bool bShowOverlay = true;

	/**
	 * Wie oft je Sekunde abgetastet wird.
	 *
	 * 4 Hz wie bei JointLedger. Eine Registrierung entsteht einmal und bleibt; was sich
	 * aendert, ist das Ein- und Ausschalten, und das geschieht in Ereignissen und nicht
	 * fuenfmal je Sekunde. Haeufiger abzutasten kostet in einer vollen Szene spuerbar Zeit —
	 * dieses Werkzeug geht JEDEN Actor und JEDE Komponente durch.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "TickLedger",
		meta = (ClampMin = "0.5"))
	float SampleHz = 4.f;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "TickLedger")
	FTickBudget Budget;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};
