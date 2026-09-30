// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "TickLedgerTypes.h"

#include "TickLedgerSubsystem.generated.h"

class UActorComponent;

/**
 * Das Auge: geht Actors und Komponenten der laufenden Welt durch und schreibt auf, was es
 * findet.
 *
 * ES URTEILT NICHT. Jede Regel steht in `UTickLedgerStatics`, und zwar als reine Funktion.
 * Diese Trennung ist der Grund, warum sich jede Regel an erfundenen Eintraegen pruefen und
 * einzeln sabotieren laesst, ohne eine Welt aufzubauen.
 */
UCLASS()
class TICKLEDGER_API UTickLedgerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTickLedgerSubsystem* Get(const UObject* WorldContext);

	// ------------------------------------------------------------ Subsystem / Tickable
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// ------------------------------------------------------------ Steuerung
	UFUNCTION(BlueprintCallable, Category = "TickLedger")
	void ResetLedger();

	/** Einmal ausserhalb des Takts abtasten — fuer Konsolenbefehle und Tests. */
	UFUNCTION(BlueprintCallable, Category = "TickLedger")
	void SampleNow();

	// ------------------------------------------------------------ Ergebnis
	UFUNCTION(BlueprintPure, Category = "TickLedger")
	const TArray<FTickRecord>& GetRecords() const { return Records; }

	/** Das geltende Budget — aus den Einstellungen, damit es nicht zwei Wahrheiten gibt. */
	UFUNCTION(BlueprintPure, Category = "TickLedger")
	FTickBudget GetBudget() const;

	/**
	 * Die Eintraege mit Befund, nach Schwere sortiert.
	 *
	 * EINE EINZIGE RANGFOLGE FUER ALLE — Kasten, Bericht und `DominantLine` fragen dieselbe
	 * Funktion. Zwei Sortierungen waeren die erste Stelle, an der Anzeige und Urteil
	 * auseinanderlaufen.
	 */
	UFUNCTION(BlueprintPure, Category = "TickLedger")
	TArray<FTickRecord> GetFindingsBySeverity() const;

	UFUNCTION(BlueprintPure, Category = "TickLedger")
	ETickVerdict GetVerdict() const;

	UFUNCTION(BlueprintPure, Category = "TickLedger")
	FString SummaryLine() const;

	/** Die auffaelligste Zeile — was ein Leser zuerst sehen soll. */
	UFUNCTION(BlueprintPure, Category = "TickLedger")
	FString DominantLine() const;

	UFUNCTION(BlueprintPure, Category = "TickLedger")
	FString LimitsLine() const;

	/** Gab es ueberhaupt ein Objekt, das ticken WOLLTE? Ein Lauf ohne eines prueft nichts. */
	UFUNCTION(BlueprintPure, Category = "TickLedger")
	bool HasSeenAnyCandidate() const { return bAnyCandidateSeen; }

	UFUNCTION(BlueprintPure, Category = "TickLedger")
	bool HasHitTrackingCap() const { return bHitCap; }

	/** Schreibt `Saved/TickLedger/report.json` und liefert den vollen Pfad. */
	UFUNCTION(BlueprintCallable, Category = "TickLedger")
	FString WriteReport(const FString& FileName = TEXT("report")) const;

	// ------------------------------------------------------------ Das Tor

	/**
	 * `bExitWhenDone = false` misst und urteilt, beendet den Prozess aber NICHT.
	 *
	 * Ohne diesen Schalter liesse sich das Tor in einer laufenden Sitzung gar nicht pruefen:
	 * jeder Versuch naehme den Editor mit, und ein Tor, das man nur im Ernstfall ausloesen
	 * kann, ist ungeprueft.
	 */
	void StartGate(float Seconds, bool bExitWhenDone = true);
	bool IsGateRunning() const { return bGateRunning; }

private:
	void SampleWorld();

	/** Einen Eintrag anlegen oder finden. Schluessel: `Owner|Object`. */
	FTickRecord* FindOrAdd(FName ObjectName, FName OwnerName, FName ClassName, bool bIsComponent);

	/**
	 * Einen Eintrag aus einer Tick-Funktion fortschreiben.
	 *
	 * DAS HOECHSTE JE GESEHENE ERGEBNIS, NICHT DAS LETZTE — deshalb `|=`. Ein Objekt, das
	 * beim Aufraeumen abgemeldet wird, war trotzdem registriert; wer den Schlusszustand
	 * nimmt, meldet jedes ordentlich beendete Objekt als nie gelaufen.
	 */
	void Fortschreiben(FTickRecord& R, bool bCanEverTick, bool bRegistered, bool bEnabled,
		float Takt);

	void FinishGate();

	UPROPERTY()
	TArray<FTickRecord> Records;

	TMap<FString, int32> Index;

	float SinceSample = 0.f;
	float ElapsedSeconds = 0.f;
	int32 SamplesTaken = 0;
	float LongestGapSeconds = 0.f;

	bool bAnyCandidateSeen = false;
	bool bHitCap = false;

	bool bGateRunning = false;
	float GateSeconds = 0.f;
	float GateElapsed = 0.f;
	bool bGateExits = true;
};
