// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "TickLedgerTypes.h"

#include "TickLedgerStatics.generated.h"

/**
 * Die Regeln — als reine Funktionen, jede einzeln pruefbar.
 *
 * KEINE EINZIGE DAVON FASST DIE WELT AN. Das Subsystem sammelt, diese Bibliothek urteilt.
 * Nur so laesst sich jede Regel im Automationstest an erfundenen Eintraegen pruefen, ohne
 * eine Welt und tickende Actors aufzubauen — und nur so kann jede Regel einmal sabotiert
 * werden, um zu zeigen, dass der Test sie wirklich festhaelt.
 */
UCLASS()
class TICKLEDGER_API UTickLedgerStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ---------------------------------------------------------------- Enthaltung zuerst

	/**
	 * Darf ueber diesen Eintrag ueberhaupt geurteilt werden?
	 *
	 * Zwei Gruende, es zu lassen, und beide sind Unwissen und nicht Unschuld:
	 *  1. Das Objekt wurde kuerzer beobachtet als `MinObservedSeconds`. Ein Actor, der in der
	 *     letzten Sekunde gespawnt wurde, hatte keine Gelegenheit — daraus einen Befund zu
	 *     machen waere eine Aussage ueber die Messdauer, nicht ueber das Objekt.
	 *  2. Es hatte nie einen Namen; dann ist der Eintrag eine Huelse.
	 */
	UFUNCTION(BlueprintPure, Category = "TickLedger|Rules")
	static bool IsJudgeable(const FTickRecord& Record, const FTickBudget& Budget);

	UFUNCTION(BlueprintPure, Category = "TickLedger|Rules")
	static bool IsUnjudged(const FTickRecord& Record, const FTickBudget& Budget);

	// ------------------------------------------------- Der Fall, der KEIN Befund ist

	/**
	 * Das Objekt will gar nicht ticken.
	 *
	 * DIE WICHTIGSTE FUNKTION IN DIESER DATEI, obwohl sie im Normalfall nichts meldet. In
	 * einer gewoehnlichen Szene trifft das auf die MEHRHEIT der Objekte zu — es ist die
	 * empfohlene Bauweise. Ein Werkzeug ohne diese Unterscheidung liefert tausend Zeilen und
	 * darin keinen einzigen Befund.
	 */
	UFUNCTION(BlueprintPure, Category = "TickLedger|Rules")
	static bool DoesNotWantToTick(const FTickRecord& Record);

	// ---------------------------------------------------------------- Die zwei Befunde

	/**
	 * Will ticken, war aber nie registriert — dann laeuft `Tick()` nie.
	 *
	 * Der Schluss ist belegt und nicht angenommen: am 29.09.2026 an zwei echten Actors
	 * gemessen, beide mit `bCanEverTick = true`; der registrierte zaehlte nach zehn Weltticks
	 * 1 Tick, der nicht registrierte 0.
	 */
	UFUNCTION(BlueprintPure, Category = "TickLedger|Rules")
	static bool IsWantsTickButNeverRegistered(const FTickRecord& Record);

	/** Registriert, in dieser Sitzung aber nie eingeschaltet. Warnung, kein Fehler. */
	UFUNCTION(BlueprintPure, Category = "TickLedger|Rules")
	static bool IsRegisteredButNeverEnabled(const FTickRecord& Record);

	/**
	 * Die Rangfolge.
	 *
	 * 1. DIE ENTHALTUNG ZUERST. Ein Objekt, das zwei Sekunden lang beobachtet wurde, sagt
	 *    ueber „nie registriert" nichts aus.
	 *
	 * 2. DANN „WILL GAR NICHT TICKEN" ALS AUSSCHLUSS, vor allen Befunden. Sonst wuerde jedes
	 *    stille Objekt als „nie registriert" durchgehen — und still ist die Mehrheit.
	 *    Derselbe Fehler, den JointLedger mit dem Welt-Anker beinahe gemacht haette.
	 *
	 * 3. DANN „nie registriert" VOR „nie eingeschaltet". Wer nie registriert war, kann auch
	 *    nie eingeschaltet gewesen sein — beide treffen zu, aber nur die erste nennt die
	 *    Ursache. „Nie eingeschaltet" schickte den Leser suchen, wo nichts zu finden ist.
	 */
	UFUNCTION(BlueprintPure, Category = "TickLedger|Rules")
	static ETickFault ClassifyRecord(const FTickRecord& Record, const FTickBudget& Budget);

	// ---------------------------------------------------------------- Zaehlen und urteilen

	UFUNCTION(BlueprintPure, Category = "TickLedger|Rules")
	static int32 CountFaults(const TArray<FTickRecord>& Records, ETickFault Fault,
		const FTickBudget& Budget);

	/**
	 * Wie viele der beurteilten Objekte in Ordnung waren, in Prozent.
	 *
	 * Der Nenner sind die BEURTEILTEN, nicht alle — sonst druecken Enthaltungen die Quote.
	 * **Gibt es keinen Nenner, gibt es keine Zahl: dann −1 und im Bericht `n/a`.** Eine 0
	 * waere eine Zahl, die niemand gemessen hat.
	 */
	UFUNCTION(BlueprintPure, Category = "TickLedger|Rules")
	static float HealthyShare(const TArray<FTickRecord>& Records, const FTickBudget& Budget);

	UFUNCTION(BlueprintPure, Category = "TickLedger|Rules")
	static ETickVerdict Judge(const TArray<FTickRecord>& Records, const FTickBudget& Budget,
		bool bAnyCandidateSeen);

	UFUNCTION(BlueprintPure, Category = "TickLedger|Rules")
	static int32 VerdictExitCode(ETickVerdict Verdict);

	UFUNCTION(BlueprintPure, Category = "TickLedger|Rules")
	static FString VerdictText(ETickVerdict Verdict);

	// ---------------------------------------------------------------- Darstellung

	UFUNCTION(BlueprintPure, Category = "TickLedger|Report")
	static FString FaultName(ETickFault Fault);

	/** Die Notiz hinter einem Eintrag — in Worten, die jemand lesen kann. */
	UFUNCTION(BlueprintPure, Category = "TickLedger|Report")
	static FString FaultText(const FTickRecord& Record, const FTickBudget& Budget);

	UFUNCTION(BlueprintPure, Category = "TickLedger|Report")
	static FString RecordLine(const FTickRecord& Record, const FTickBudget& Budget);

	UFUNCTION(BlueprintPure, Category = "TickLedger|Report")
	static FString RecordHeader();

	UFUNCTION(BlueprintPure, Category = "TickLedger|Report")
	static FLinearColor VerdictColour(ETickVerdict Verdict);

	UFUNCTION(BlueprintPure, Category = "TickLedger|Report")
	static FLinearColor FaultColour(ETickFault Fault);

	/** Die Grenzen dieses Werkzeugs, im Klartext und nicht in einer Fussnote. */
	UFUNCTION(BlueprintPure, Category = "TickLedger|Report")
	static FString ExplainLimits(const FTickBudget& Budget);
};
