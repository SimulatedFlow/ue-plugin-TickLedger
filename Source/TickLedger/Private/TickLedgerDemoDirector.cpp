// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "TickLedgerDemoDirector.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "TickLedgerDemoProbe.h"
#include "TickLedgerHUD.h"
#include "TickLedgerLog.h"
#include "TickLedgerStatics.h"
#include "TickLedgerSubsystem.h"

namespace
{
	const TCHAR* const WuerfelPfad = TEXT("/Engine/BasicShapes/Cube.Cube");
	// Das EINZIGE Engine-Material mit einem Farbparameter, den man setzen kann. Der
	// Engine-Wuerfel traegt `WorldGridMaterial`, und das hat gar keine Parameter — am
	// 29.09.2026 lief `SetVectorParameterValue` deshalb stillschweigend ins Leere.
	const TCHAR* const FarbMaterial = TEXT("/Engine/BasicShapes/BasicShapeMaterial");

	/** Der Name der Komponente, an der derselbe Fehler eine Ebene tiefer sitzt. */
	const TCHAR* const KaputteKomponente = TEXT("Comp_NeverRegistered");

	/**
	 * Der Standort einer Station.
	 *
	 * AN EINER STELLE, WEIL ER ZWEIMAL GEBRAUCHT WIRD — und das hat am 30.09.2026 Geld gekostet.
	 * `AActor::FinishSpawning(T)` SETZT `T`; es laesst die Transformation des aufgeschobenen
	 * Erzeugens nicht stehen. Mit `FTransform::Identity` landeten alle vier Sonden im Ursprung,
	 * uebereinander.
	 *
	 * Gemerkt hat es NUR das Bild. Buehnenpruefung, Hauptbuch und Tick-Zaehler sagten alle
	 * „5 von 5" — keiner von ihnen sieht Geometrie an, und dem Bericht ist der Standort egal.
	 */
	FTransform StationsOrt(float X)
	{
		return FTransform(FRotator::ZeroRotator, FVector(X, 0.f, 0.f));
	}
}

ATickLedgerDemoDirector::ATickLedgerDemoDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	SceneShot = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneShot"));
	SceneShot->SetupAttachment(GetRootComponent());
	SceneShot->bCaptureEveryFrame = false;
	SceneShot->bCaptureOnMovement = false;
	SceneShot->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneShot->bAlwaysPersistRenderingState = true;
	SceneShot->PrimaryComponentTick.bCanEverTick = false;
}

void ATickLedgerDemoDirector::BeginPlay()
{
	Super::BeginPlay();
	BuildStage();
	LogStageState();
}

ATickLedgerDemoProbe* ATickLedgerDemoDirector::SpawnProbe(const TCHAR* Name, float X)
{
	UWorld* Welt = GetWorld();
	if (!Welt)
	{
		return nullptr;
	}

	// AUFGESCHOBEN ERZEUGEN — das ist der ganze Trick dieser Karte.
	//
	// `bDeferConstruction` haelt den Actor zwischen Konstruktor und `FinishSpawning` an. Genau
	// in dieser Luecke stehen die Tick-Flaggen so, wie die Anmeldung sie spaeter LIEST. Danach
	// daran zu drehen ist zu spaet — und das ist der Fehler, den dieses Plugin findet.
	//
	// NICHT `SpawnActorDeferred`: die Vorlage nimmt keine `FActorSpawnParameters` und damit
	// keinen NAMEN. Die Eintraege hiessen dann `TickLedgerDemoProbe_0` bis `_3`, und ein Bericht,
	// in dem man die Faelle nicht auseinanderhalten kann, fuehrt nichts vor.
	FActorSpawnParameters P;
	P.Name = FName(Name);
	P.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	P.Owner = this;
	P.bDeferConstruction = true;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATickLedgerDemoProbe* A = Welt->SpawnActor<ATickLedgerDemoProbe>(
		ATickLedgerDemoProbe::StaticClass(), StationsOrt(X), P);
#if WITH_EDITOR
	// NUR IM EDITOR. `SetActorLabel` steht hinter `WITH_EDITOR`, und der Paketbau uebersetzt
	// dieses Modul auch fuer Win64 ohne Editor — ohne die Klammer waere das ein Fehler, den erst
	// der Kaeufer sieht. Auf den Bericht hat das Etikett keinen Einfluss: das Hauptbuch
	// schluesselt auf `GetFName()`, und der Name kommt aus den Spawn-Parametern.
	if (A)
	{
		A->SetActorLabel(Name);
	}
#endif
	return A;
}

void ATickLedgerDemoDirector::BuildStage()
{
	if (bBuilt)
	{
		return;
	}

	UStaticMesh* Wuerfel = LoadObject<UStaticMesh>(nullptr, WuerfelPfad);
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, FarbMaterial);
	if (!Wuerfel || !Material)
	{
		// LAUT ABBRECHEN, NICHT STILL WEITERLAUFEN. Eine Demo ohne Assets zeigt „0 Befunde" und
		// sieht aus wie ein sauberer Lauf.
		UE_LOG(LogTickLedger, Error,
			TEXT("TickLedger demo: assets missing (cube %d, material %d) - the stage was NOT "
				 "built, and an empty stage is not a clean run."),
			Wuerfel ? 1 : 0, Material ? 1 : 0);
		return;
	}

	// ========================================================================================
	// DIE X-WERTE LAUFEN ABSICHTLICH RUECKWAERTS: +600 ganz LINKS, -600 ganz RECHTS.
	// ========================================================================================
	//
	// Die Aufnahme steht bei y = -980 und blickt in **+Y**. In dieser Richtung ist die rechte
	// Bildseite **-X**, und wer die Stationen in aufsteigendem X aufstellt, bekommt sie
	// spiegelverkehrt. Bei SlotLedger und bei JointLedger ist das je einmal passiert: der Kasten
	// schrieb „left to right: healthy, ..." und im Bild stand es umgekehrt.
	//
	// Eine Beschriftung, die nicht zum Bild passt, ist schlimmer als gar keine — sie fuehrt den
	// Leser zuverlaessig in die Irre.
	// ========================================================================================

	// ------------------------------------------------- 1. gesund (links)
	Healthy = SpawnProbe(TEXT("Probe_Healthy"), 600.f);
	if (Healthy)
	{
		Healthy->Configure(Wuerfel, Material);
		// Nichts anfassen: `bCanEverTick` steht aus dem Konstruktor auf true, also wird
		// angemeldet und eingeschaltet.
		Healthy->FinishSpawning(StationsOrt(600.f));
	}

	// ------------------------------------------------- 2. will ticken, nie angemeldet (FEHLER)
	NeverRegistered = SpawnProbe(TEXT("Probe_NeverRegistered"), 200.f);
	if (NeverRegistered)
	{
		NeverRegistered->Configure(Wuerfel, Material);

		// SO SIEHT DIE ANMELDUNG IHN: will nicht ticken. Also wird nichts angemeldet.
		NeverRegistered->PrimaryActorTick.bCanEverTick = false;
		NeverRegistered->FinishSpawning(StationsOrt(200.f));

		// UND JETZT IST ES ZU SPAET.
		//
		// `AActor::RegisterActorTickFunctions` ist vorbei; sie lief in `FinishSpawning` und hat
		// die Flagge auf false gesehen. Das Umlegen hier aendert nur noch, was die Flagge SAGT
		// — nicht, was angemeldet ist. Die Engine meldet keinen Fehler, im Protokoll steht keine
		// Zeile, `Tick` laeuft nie, und der Wuerfel bleibt stehen.
		//
		// Im echten Projekt sieht dieser Fehler harmloser aus: dort steht die Zuweisung in
		// `BeginPlay`, in einem Initialisierungsaufruf oder hinter einer Bedingung, die beim
		// Anmelden noch nicht erfuellt war.
		NeverRegistered->PrimaryActorTick.bCanEverTick = true;
	}

	// ------------------------------------------------- 3. angemeldet, nie eingeschaltet (WARNUNG)
	NeverEnabled = SpawnProbe(TEXT("Probe_NeverEnabled"), -200.f);
	if (NeverEnabled)
	{
		NeverEnabled->Configure(Wuerfel, Material);

		// BEIDES, UND NICHT NUR `bStartWithTickEnabled`.
		//
		// `RegisterActorTickFunctions` schaltet mit `bStartWithTickEnabled ||
		// IsTickFunctionEnabled()` ein — und `IsTickFunctionEnabled()` ist bei einer noch nicht
		// angemeldeten Funktion standardmaessig WAHR. Die erste Flagge allein waere also
		// wirkungslos, und die Station waere in Wahrheit gesund.
		//
		// Die Alternative — nach `FinishSpawning` `SetActorTickEnabled(false)` — waere ein
		// Wettrennen gegen den Abtaster: haette der einmal hingesehen, stuende
		// `bWasEverEnabled` schon auf true und die Station waere zu Recht unauffaellig.
		NeverEnabled->PrimaryActorTick.bStartWithTickEnabled = false;
		NeverEnabled->PrimaryActorTick.SetTickFunctionEnable(false);
		NeverEnabled->FinishSpawning(StationsOrt(-200.f));
	}

	// ------------------------------------------------- 4. will gar nicht ticken (KEIN BEFUND)
	Silent = SpawnProbe(TEXT("Probe_Silent"), -600.f);
	if (Silent)
	{
		Silent->Configure(Wuerfel, Material);
		// UND ES BLEIBT DABEI. Das ist der wichtigste Fall der Karte, obwohl er nichts meldet:
		// er sieht Fall 2 und Fall 3 zum Verwechseln aehnlich — ein stillstehender Wuerfel —,
		// und ein Werkzeug ohne diese Unterscheidung wuerde die Mehrheit jeder Szene
		// anschwaerzen.
		Silent->PrimaryActorTick.bCanEverTick = false;
		Silent->FinishSpawning(StationsOrt(-600.f));
	}

	Probes = { Healthy, NeverRegistered, NeverEnabled, Silent };

	// ------------------------------------------------- 5. derselbe Fehler auf der KOMPONENTE
	//
	// Keine eigene Station, sondern ein kleiner Wuerfel am Fuss von Station 2 — die Komponente
	// ist die unauffaelligere Haelfte, und der Bericht muss sie mit ihrem Besitzer zeigen.
	BrokenComponent = NewObject<UStaticMeshComponent>(this, FName(KaputteKomponente));
	if (BrokenComponent)
	{
		BrokenComponent->SetupAttachment(RootComponent);
		BrokenComponent->SetRelativeLocation(FVector(200.f, -170.f, 25.f));
		BrokenComponent->SetRelativeScale3D(FVector(0.5f));
		BrokenComponent->SetStaticMesh(Wuerfel);
		BrokenComponent->SetMaterial(0, Material);
		BrokenComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Dieselbe Reihenfolge eine Ebene tiefer: `UActorComponent::RegisterComponentTickFunctions`
		// liest `bCanEverTick` beim Anmelden, und `RegisterComponent` ist danach vorbei.
		BrokenComponent->PrimaryComponentTick.bCanEverTick = false;
		BrokenComponent->RegisterComponent();
		BrokenComponent->PrimaryComponentTick.bCanEverTick = true;
	}

	bBuilt = true;
	UE_LOG(LogTickLedger, Display,
		TEXT("TickLedger demo: four stations - healthy, NEVER REGISTERED (error), never enabled "
			 "(warning), silent (NOT a finding) - plus '%s' with the same fault on a component. "
			 "Each station is checked BY NAME against the verdict it should get; the world-wide "
			 "counts are context only, because other actors may have findings of their own."),
		KaputteKomponente);
}

void ATickLedgerDemoDirector::LogStageState()
{
	// WAS DIE ENGINE SAGT, VOR DEM ERSTEN WORT DES PLUGINS.
	//
	// Das ist nicht doppelt gemoppelt: stimmten Regisseur und Plugin ueberein, die Buehne stuende
	// aber gar nicht im gemeinten Zustand, waere die Demo eine Behauptung mit zwei Zeugen. Genau
	// so ist die Machbarkeitsprobe dieses Plugins einmal falsch gruen geworden — in einer
	// selbstgebauten Welt war NICHTS angemeldet, und beide Sonden zaehlten 0.
	auto Zeile = [](const TCHAR* Name, const FTickFunction& F)
	{
		UE_LOG(LogTickLedger, Display,
			TEXT("TickLedger demo stage: %-24s wants=%d registered=%d enabled=%d"),
			Name, F.bCanEverTick ? 1 : 0, F.IsTickFunctionRegistered() ? 1 : 0,
			F.IsTickFunctionEnabled() ? 1 : 0);
	};

	for (ATickLedgerDemoProbe* P : Probes)
	{
		if (P)
		{
			Zeile(*P->GetName(), P->PrimaryActorTick);
		}
	}
	if (BrokenComponent)
	{
		Zeile(KaputteKomponente, BrokenComponent->PrimaryComponentTick);
	}

	// Und jetzt die Gegenprobe gegen die ABSICHT. Ein falsch aufgebauter Fall soll hier
	// auffallen und nicht erst im Bericht.
	auto Pruefe = [](const TCHAR* Name, const FTickFunction* F, bool bWants, bool bRegd,
		bool bEnab) -> bool
	{
		if (!F)
		{
			UE_LOG(LogTickLedger, Error, TEXT("TickLedger demo: '%s' was not built."), Name);
			return false;
		}
		const bool bOk = (F->bCanEverTick == bWants)
			&& (F->IsTickFunctionRegistered() == bRegd)
			&& (F->IsTickFunctionEnabled() == bEnab);
		if (!bOk)
		{
			UE_LOG(LogTickLedger, Error,
				TEXT("TickLedger demo: '%s' is NOT in the intended state - wanted "
					 "wants=%d regd=%d enab=%d, got wants=%d regd=%d enab=%d. The map does not "
					 "show what it claims."),
				Name, bWants ? 1 : 0, bRegd ? 1 : 0, bEnab ? 1 : 0,
				F->bCanEverTick ? 1 : 0, F->IsTickFunctionRegistered() ? 1 : 0,
				F->IsTickFunctionEnabled() ? 1 : 0);
		}
		return bOk;
	};

	int32 Gut = 0;
	Gut += Pruefe(TEXT("Probe_Healthy"), Healthy ? &Healthy->PrimaryActorTick : nullptr,
		true, true, true) ? 1 : 0;
	Gut += Pruefe(TEXT("Probe_NeverRegistered"),
		NeverRegistered ? &NeverRegistered->PrimaryActorTick : nullptr, true, false, false) ? 1 : 0;
	Gut += Pruefe(TEXT("Probe_NeverEnabled"),
		NeverEnabled ? &NeverEnabled->PrimaryActorTick : nullptr, true, true, false) ? 1 : 0;
	Gut += Pruefe(TEXT("Probe_Silent"), Silent ? &Silent->PrimaryActorTick : nullptr,
		false, false, false) ? 1 : 0;
	Gut += Pruefe(KaputteKomponente,
		BrokenComponent ? &BrokenComponent->PrimaryComponentTick : nullptr, true, false, false)
		? 1 : 0;

	UE_LOG(LogTickLedger, Display,
		TEXT("TickLedger demo stage: %d of 5 stations are in the intended state (measured on the "
			 "engine's own tick functions, not on the ledger)."), Gut);
}

void ATickLedgerDemoDirector::ColourPillars()
{
	UTickLedgerSubsystem* S = UTickLedgerSubsystem::Get(this);
	if (!S)
	{
		return;
	}
	const FTickBudget Budget = S->GetBudget();

	for (ATickLedgerDemoProbe* P : Probes)
	{
		if (!P)
		{
			continue;
		}
		// DIE FARBE KOMMT AUS DEM URTEIL DES PLUGINS, nicht aus der Erwartung des Regisseurs.
		// Eine Demo, die nach ihrer eigenen Annahme einfaerbt, sieht auch dann richtig aus, wenn
		// das Plugin falsch liegt.
		ETickFault Befund = ETickFault::Unjudged;
		for (const FTickRecord& R : S->GetRecords())
		{
			if (!R.bIsComponent && R.ObjectName == P->GetFName())
			{
				Befund = UTickLedgerStatics::ClassifyRecord(R, Budget);
				break;
			}
		}
		P->SetVerdictColour(UTickLedgerStatics::FaultColour(Befund));
	}
}

void ATickLedgerDemoDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuilt)
	{
		return;
	}

	Elapsed += DeltaSeconds;
	ColourPillars();

	if (bTakeShots)
	{
		SinceShot += DeltaSeconds;
		if (SinceShot >= ShotEverySeconds)
		{
			SinceShot = 0.f;
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				if (ATickLedgerHUD* HUD = Cast<ATickLedgerHUD>(PC->GetHUD()))
				{
					// DIE ZAHLEN IM SATZ SIND GEZAEHLT, NICHT BEHAUPTET: sie kommen aus den
					// Zaehlern der Sonden, und die laufen nur hoch, wenn `Tick` wirklich lief.
					HUD->Note = FString::Printf(
						TEXT("demo shot %d at %.1f s - left to right: healthy, NEVER REGISTERED "
							 "(error), never enabled (warning), silent (no finding). Ticks so "
							 "far: %d / %d / %d / %d. THREE cubes stand still and the ledger "
							 "gives them three different verdicts - pillars are coloured by the "
							 "PLUGIN's verdict."),
						Shots, Elapsed,
						Healthy ? Healthy->TickCount : -1,
						NeverRegistered ? NeverRegistered->TickCount : -1,
						NeverEnabled ? NeverEnabled->TickCount : -1,
						Silent ? Silent->TickCount : -1);
				}
			}
			SaveSceneImage(1920, 1080,
				FString::Printf(TEXT("TickLedger_Shot_%02d"), Shots));
			++Shots;
		}
	}

	if (!bReported && Elapsed >= RunSeconds)
	{
		bReported = true;
		LogComparison();
	}
}

void ATickLedgerDemoDirector::LogComparison()
{
	UTickLedgerSubsystem* S = UTickLedgerSubsystem::Get(this);
	if (!S)
	{
		return;
	}
	const FTickBudget Budget = S->GetBudget();

	// ================================================================================
	// JEDE STATION EINZELN — UND AUSDRUECKLICH NICHT DIE GANZE WELT.
	// ================================================================================
	// Der erste Versuch am 30.09.2026 zaehlte mit `CountFaults` ueber alle Eintraege und
	// erwartete genau 1 „never enabled". Gemeldet wurden 2. Der zweite war
	// `GameplayDebuggerCategoryReplicator_0` — ein Actor der ENGINE, der seine Tick-Funktion
	// anmeldet und nur einschaltet, wenn der Debugger laeuft. Also genau der Fall, den dieses
	// Plugin als Warnung und nicht als Fehler fuehrt: ein Actor, der auf seinen Einsatz wartet,
	// sieht aus wie einer, dessen Einsatz nie kam.
	//
	// Das Plugin lag richtig, die Gegenprobe lag falsch. Eine Demo, die ihre Umgebung mitzaehlt,
	// schlaegt Alarm, sobald jemand einen Actor auf die Karte stellt — und ein Alarm, der von
	// fremden Objekten abhaengt, wird nach dem zweiten Mal ignoriert.
	//
	// Darum wird jetzt namentlich verglichen: fuenf Stationen, fuenf erwartete Urteile.
	// ================================================================================
	struct FErwartung
	{
		const TCHAR* Objekt;
		FName Besitzer;
		ETickFault Soll;
	};
	const FErwartung Erwartungen[] = {
		{ TEXT("Probe_Healthy"),         NAME_None,      ETickFault::None },
		{ TEXT("Probe_NeverRegistered"), NAME_None,      ETickFault::WantsTickButNeverRegistered },
		{ TEXT("Probe_NeverEnabled"),    NAME_None,      ETickFault::RegisteredButNeverEnabled },
		// Still ist KEIN Befund — bei der Voreinstellung `bDoesNotWantToTickIsAFinding = false`
		// ist das Urteil `None`, dasselbe wie beim gesunden. Zwei voellig verschiedene Bilder,
		// ein Urteil, und beides ist richtig.
		{ TEXT("Probe_Silent"),          NAME_None,      ETickFault::None },
		{ TEXT("Comp_NeverRegistered"),  GetFName(),     ETickFault::WantsTickButNeverRegistered }
	};

	int32 WieErwartet = 0;
	for (const FErwartung& E : Erwartungen)
	{
		const FName Gesucht(E.Objekt);
		const FTickRecord* Treffer = nullptr;
		for (const FTickRecord& R : S->GetRecords())
		{
			if (R.ObjectName == Gesucht && R.OwnerName == E.Besitzer)
			{
				Treffer = &R;
				break;
			}
		}
		if (!Treffer)
		{
			UE_LOG(LogTickLedger, Error,
				TEXT("TickLedger demo: station '%s' is not in the ledger at all."), E.Objekt);
			continue;
		}
		const ETickFault Ist = UTickLedgerStatics::ClassifyRecord(*Treffer, Budget);
		if (Ist == E.Soll)
		{
			++WieErwartet;
		}
		else
		{
			UE_LOG(LogTickLedger, Error,
				TEXT("TickLedger demo: station '%s' - director expected '%s', the ledger says "
					 "'%s'."),
				E.Objekt, *UTickLedgerStatics::FaultName(E.Soll),
				*UTickLedgerStatics::FaultName(Ist));
		}
	}

	// Die weltweiten Zahlen bleiben im Protokoll, aber als UMGEBUNG und nicht als Pruefung:
	// fremde Objekte duerfen eigene Befunde haben, und der Replicator hat einen.
	const int32 Stumm = UTickLedgerStatics::CountFaults(
		S->GetRecords(), ETickFault::WantsTickButNeverRegistered, Budget);
	const int32 Schlafend = UTickLedgerStatics::CountFaults(
		S->GetRecords(), ETickFault::RegisteredButNeverEnabled, Budget);

	UE_LOG(LogTickLedger, Display,
		TEXT("TickLedger demo: %d of %d stations judged exactly as the director intended. "
			 "World-wide (context, not a check): %d never-registered, %d never-enabled - objects "
			 "outside the stage may legitimately have findings of their own."),
		WieErwartet, static_cast<int32>(UE_ARRAY_COUNT(Erwartungen)), Stumm, Schlafend);

	// ================================================================================
	// DIE GEGENPROBE, DIE AUS DER DEMO EINEN BELEG MACHT.
	// ================================================================================
	// Bis hierher sagen zwei Stellen dasselbe — und beide lesen dieselben Flaggen der Engine.
	// Die Zaehler der Sonden tun das NICHT: sie laufen in `Tick` hoch und sonst nirgends. Sie
	// sind der einzige Zeuge, der von `IsTickFunctionRegistered()` unabhaengig ist.
	//
	// Genau diese Unabhaengigkeit hat bei SlotLedger gefehlt: dort stimmte jede Einzelmessung,
	// und am Ende kam trotzdem nichts an.
	// ================================================================================
	const int32 TicksGesund = Healthy ? Healthy->TickCount : -1;
	const int32 TicksStumm = NeverRegistered ? NeverRegistered->TickCount : -1;
	const int32 TicksSchlafend = NeverEnabled ? NeverEnabled->TickCount : -1;
	const int32 TicksStill = Silent ? Silent->TickCount : -1;

	UE_LOG(LogTickLedger, Display,
		TEXT("TickLedger demo: measured ticks - healthy %d, never-registered %d, never-enabled "
			 "%d, silent %d (counted inside Tick, independent of the ledger)."),
		TicksGesund, TicksStumm, TicksSchlafend, TicksStill);

	const bool bZaehlerStimmen = (TicksGesund > 0) && (TicksStumm == 0)
		&& (TicksSchlafend == 0) && (TicksStill == 0);

	const bool bAlleStationen = (WieErwartet == static_cast<int32>(UE_ARRAY_COUNT(Erwartungen)));

	if (!bAlleStationen)
	{
		UE_LOG(LogTickLedger, Error,
			TEXT("TickLedger demo: the director and the plugin DISAGREE on a station it built "
				 "itself. That is a finding against the plugin, not against the map."));
	}
	if (!bZaehlerStimmen)
	{
		UE_LOG(LogTickLedger, Error,
			TEXT("TickLedger demo: the tick counters contradict the ledger. Expected the healthy "
				 "probe above 0 and the other three at exactly 0. Either the stage is wrong or "
				 "the ledger is - and a green report would have hidden it."));
	}
	if (bAlleStationen && bZaehlerStimmen)
	{
		UE_LOG(LogTickLedger, Display,
			TEXT("TickLedger demo: ledger, director and the independent tick counters all agree."));
	}
}

FString ATickLedgerDemoDirector::SaveSceneImage(int32 Width, int32 Height,
	const FString& FileName)
{
	UWorld* Welt = GetWorld();
	if (!Welt || !SceneShot || Width < 16 || Height < 16)
	{
		return FString();
	}
	// OHNE RHI KEIN BILD — und das wird GESAGT, nicht stillschweigend uebergangen.
	// `FImageUtils::GetRenderTargetImage` liest ein Render-Target, das in einem
	// kopflosen Lauf keine Ressource hat: EXCEPTION_ACCESS_VIOLATION auf 0x0, Shell=3
	// und kein Rueckgabewert im Protokoll. Die Messung und das Tor bleiben unberuehrt.
	if (!FApp::CanEverRender())
	{
		UE_LOG(LogTickLedger, Warning,
			TEXT("TickLedger: no RHI (headless) - skipping the image. ")
			TEXT("The measurement and the gate are unaffected."));
		return FString();
	}

	UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this);
	RT->RenderTargetFormat = RTF_RGBA8;
	RT->ClearColor = FLinearColor::Black;
	RT->bAutoGenerateMips = false;
	RT->InitAutoFormat(Width, Height);
	RT->UpdateResourceImmediate(true);
	SceneShot->TextureTarget = RT;

	SceneShot->SetWorldLocationAndRotation(ShotLocation, (ShotTarget - ShotLocation).Rotation());
	SceneShot->FOVAngle = ShotFOV;

	// ZWEIMAL. Der erste Aufruf baut den Renderzustand auf, der zweite liefert das Bild.
	SceneShot->CaptureScene();
	SceneShot->CaptureScene();

	// Der Kasten kommt in DASSELBE Ziel — ein eigenes haette durchsichtigen Grund.
	APlayerController* PC = Welt->GetFirstPlayerController();
	ATickLedgerHUD* HUD = PC ? Cast<ATickLedgerHUD>(PC->GetHUD()) : nullptr;
	if (HUD)
	{
		UCanvas* C = nullptr;
		FVector2D Groesse = FVector2D::ZeroVector;
		FDrawToRenderTargetContext Kontext;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(Welt, RT, C, Groesse, Kontext);
		if (C)
		{
			HUD->DrawPanel(C, FVector2D(24.f, 24.f),
				FMath::Min(static_cast<float>(Width) - 48.f, HUD->PanelWidth));
		}
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(Welt, Kontext);
	}
	else
	{
		UE_LOG(LogTickLedger, Warning,
			TEXT("TickLedger demo: no TickLedgerHUD on the player controller, the shot will have "
				 "no panel. Is the game mode set on this map?"));
	}

	const FString Ordner = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("TickLedger") / TEXT("Shots"));
	const FString Voll = Ordner / ((FileName.IsEmpty() ? TEXT("Scene") : FileName) + TEXT(".png"));

	// REGEL 3.
	FImage Bild;
	TArray64<uint8> PNG;
	if (!FImageUtils::GetRenderTargetImage(RT, Bild)
		|| !FImageUtils::CompressImage(PNG, TEXT("PNG"), Bild)
		|| !FFileHelper::SaveArrayToFile(TArrayView64<const uint8>(PNG.GetData(), PNG.Num()), *Voll))
	{
		UE_LOG(LogTickLedger, Warning, TEXT("TickLedger: could not write %s"), *Voll);
		return FString();
	}
	UE_LOG(LogTickLedger, Display, TEXT("TickLedger: scene image -> %s"), *Voll);
	return Voll;
}
