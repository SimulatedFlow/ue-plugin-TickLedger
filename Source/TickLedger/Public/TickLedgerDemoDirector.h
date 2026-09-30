// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TickLedgerDemoDirector.generated.h"

class ATickLedgerDemoProbe;
class USceneCaptureComponent2D;
class UStaticMeshComponent;

/**
 * Die Buehne — VIER Stationen nebeneinander, und der Unterschied ist zu SEHEN.
 *
 *   1. `Probe_Healthy`           tickt, der Wuerfel steigt und dreht sich   -> kein Befund
 *   2. `Probe_NeverRegistered`   will ticken, ist nie angemeldet            -> FEHLER
 *   3. `Probe_NeverEnabled`      angemeldet, nie eingeschaltet              -> Warnung
 *   4. `Probe_Silent`            will gar nicht ticken                      -> KEIN Befund
 *
 * DAZU EIN FUENFTER FALL OHNE EIGENE STATION: `Comp_NeverRegistered` haengt an diesem Actor und
 * hat denselben Fehler auf KOMPONENTENEBENE. Das ist die unauffaelligere Haelfte — ein Werkzeug,
 * das nur Actors ansieht, findet den haeufigeren Fall nicht.
 *
 * ========================================================================================
 * WAS DIESE KARTE ZEIGT UND KEINE ANDERE ZEIGEN KANN:
 * ========================================================================================
 * DREI DER VIER WUERFEL STEHEN STILL — und das Hauptbuch gibt ihnen DREI VERSCHIEDENE Urteile:
 * Fehler, Warnung, kein Befund. Das Auge sieht dreimal dasselbe. Genau dieser Unterschied ist
 * das Produkt; waeren die drei unterscheidbar, braeuchte niemand das Werkzeug.
 *
 * DIE FAELLE WERDEN ECHT HERGESTELLT, NICHT GESTELLT. Fall 2 entsteht so, wie er im Projekt
 * entsteht: `bCanEverTick` steht beim Anmelden auf false und wird DANACH eingeschaltet. Die
 * Engine meldet dazu nichts, im Protokoll steht keine Zeile, und `Tick` laeuft nie wieder. Kein
 * Schalter im Plugin faelscht diesen Zustand nach — deshalb belegt die Karte etwas.
 * ========================================================================================
 */
UCLASS()
class TICKLEDGER_API ATickLedgerDemoDirector : public AActor
{
	GENERATED_BODY()

public:
	ATickLedgerDemoDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Ein Szenenbild ueber ein Render-Target — Regel 3, nie `ExportRenderTarget`. */
	UFUNCTION(BlueprintCallable, Category = "TickLedger|Demo")
	FString SaveSceneImage(int32 Width, int32 Height, const FString& FileName);

	/**
	 * Wie lange der Regisseur laeuft, bevor er seine eigene Zaehlung meldet.
	 *
	 * KLEINER ALS DIE TORLAUFZEIT, und das ist keine Feinheit: beim ersten Lauf stand hier 20 s
	 * und das Tor lief 14 s. Das Tor beendet den Prozess mit `Force=true` — die Gegenprobe kam
	 * also nie dran, und der Lauf sah trotzdem vollstaendig aus. Zehn Sekunden reichen: der
	 * Zustand, um den es geht, steht nach dem ersten Tick fest.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger|Demo")
	float RunSeconds = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger|Demo")
	bool bTakeShots = false;

	/**
	 * Abstand zwischen zwei Bildern.
	 *
	 * BEWUSST KEIN TEILER VON `RunSeconds` und kein runder Wert — siehe die Begruendung in
	 * `TickLedgerDemoProbe::Tick`. Hier kommt dazu, dass die Drehung des gesunden Wuerfels bei
	 * 80 Grad je Sekunde und einem Takt von 4,5 s fast eine ganze Umdrehung waere.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger|Demo")
	float ShotEverySeconds = 2.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger|Demo")
	FVector ShotLocation = FVector(0.f, -1150.f, 300.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger|Demo")
	FVector ShotTarget = FVector(0.f, 0.f, 160.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger|Demo")
	float ShotFOV = 70.f;

private:
	void BuildStage();

	/**
	 * Was die ENGINE ueber die vier Stationen sagt — vor dem ersten Wort des Plugins.
	 *
	 * Ohne diesen Schritt beweist ein uebereinstimmender Bericht nichts: stimmten Regisseur und
	 * Plugin ueberein, die Buehne stuende aber gar nicht im gemeinten Zustand, waere die ganze
	 * Demo eine Behauptung mit zwei Zeugen.
	 */
	void LogStageState();
	void ColourPillars();
	void LogComparison();

	ATickLedgerDemoProbe* SpawnProbe(const TCHAR* Name, float X);

	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> SceneShot;

	/** Die Station, deren Fehler auf der Komponente sitzt statt auf dem Actor. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BrokenComponent;

	UPROPERTY()
	TObjectPtr<ATickLedgerDemoProbe> Healthy;

	UPROPERTY()
	TObjectPtr<ATickLedgerDemoProbe> NeverRegistered;

	UPROPERTY()
	TObjectPtr<ATickLedgerDemoProbe> NeverEnabled;

	UPROPERTY()
	TObjectPtr<ATickLedgerDemoProbe> Silent;

	UPROPERTY()
	TArray<TObjectPtr<ATickLedgerDemoProbe>> Probes;

	bool bBuilt = false;
	bool bReported = false;
	float Elapsed = 0.f;
	float SinceShot = 0.f;
	int32 Shots = 0;
};
