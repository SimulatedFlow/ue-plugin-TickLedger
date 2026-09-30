// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TickLedgerTickProbe.generated.h"

/**
 * Ein Actor, der seine eigenen Ticks zaehlt — die Gegenprobe zu allem, was dieses Plugin misst.
 *
 * WARUM ER IM PRODUKT LIEGT UND NICHT NUR IM TEST: `IsTickFunctionRegistered()` ist eine
 * Aussage der Engine ueber sich selbst. Sie zu glauben waere derselbe Fehler, den SlotLedger
 * zwei Tage lang gemacht hat — dort stimmte jede Einzelmessung, und trotzdem kam am Ende
 * nichts an. Ein Zaehler, der unabhaengig davon hochlaeuft, ist der einzige Weg, „ist
 * registriert" gegen „laeuft wirklich" zu halten.
 *
 * Er zaehlt und tut sonst nichts: kein Mesh, kein Material, keine Logik.
 */
UCLASS()
class TICKLEDGER_API ATickLedgerTickProbe : public AActor
{
	GENERATED_BODY()

public:
	ATickLedgerTickProbe();

	virtual void Tick(float DeltaSeconds) override;

	/** Wie oft `Tick` wirklich gelaufen ist. */
	UPROPERTY(BlueprintReadOnly, Category = "TickLedger")
	int32 TickCount = 0;
};
