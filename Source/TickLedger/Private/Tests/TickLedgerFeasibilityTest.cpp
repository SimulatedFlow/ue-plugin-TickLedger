// Copyright 2026 Silvan Teufel. All Rights Reserved.
//
// DIE MACHBARKEITSPROBE — sie steht VOR dem Plugin und entscheidet, ob es eines geben kann.
//
// Behauptet wird dreierlei, und keines davon wird geglaubt:
//   1. `bCanEverTick = true` und `IsTickFunctionRegistered() = false` koennen GLEICHZEITIG
//      vorkommen. Wenn nicht, gaebe es den Befund nicht, den dieses Plugin verkaufen soll.
//   2. In diesem Zustand laeuft `Tick()` wirklich nie — mit einem eigenen Zaehler gegengeprueft
//      und nicht aus der Registrierung geschlossen.
//   3. Der gesunde Fall tickt tatsaechlich. Ohne diese Haelfte wuerde die Probe auch dann
//      gruenen, wenn in dieser Testwelt UEBERHAUPT NICHTS tickt — und dann haette sie nichts
//      gemessen ausser sich selbst.

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TickLedgerTickProbe.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TickLedgerProbeTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTickLedgerFeasibilityTest,
	"TickLedger.Feasibility.CanEverTickWithoutRegistration",
	TickLedgerProbeTests::TestFlags)

bool FTickLedgerFeasibilityTest::RunTest(const FString&)
{
	// Eine eigene Welt, damit der Test niemandem in die laufende Sitzung greift.
	UWorld* Welt = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	if (!Welt)
	{
		AddError(TEXT("Keine Testwelt — ohne sie ist nichts zu messen."));
		return false;
	}
	FWorldContext& Kontext = GEngine->CreateNewWorldContext(EWorldType::Game);
	Kontext.SetCurrentWorld(Welt);
	Welt->InitializeActorsForPlay(FURL());
	Welt->BeginPlay();

	// ------------------------------------------------------------------ der gesunde Fall
	ATickLedgerTickProbe* Gesund = Welt->SpawnActor<ATickLedgerTickProbe>();
	TestNotNull(TEXT("der gesunde Actor wurde gespawnt"), Gesund);

	// ERST DAS MESSGERAET PRUEFEN, DANN DAMIT MESSEN.
	//
	// Der erste Lauf dieser Probe meldete fuer BEIDE Actors „registriert=0" und 0 Ticks. Die
	// eigentliche Behauptung — `bCanEverTick=true` bei fehlender Registrierung — waere damit
	// gruen gewesen, und zwar aus dem denkbar schlechtesten Grund: in dieser handgebauten
	// Welt registriert NIEMAND seine Tick-Funktion. Der Test haette nur sich selbst gemessen.
	//
	// Eine frisch erzeugte Welt laeuft nicht von allein an; was ein Spielstart tut, muss hier
	// von Hand geschehen. Deshalb wird der Zustand ausdruecklich hergestellt und nachgelesen,
	// statt ihn vorauszusetzen.
	AddInfo(FString::Printf(TEXT("Welt: HasBegunPlay=%d, bIsWorldInitialized=%d"),
		Welt->HasBegunPlay() ? 1 : 0, Welt->bIsWorldInitialized ? 1 : 0));
	if (Gesund)
	{
		Gesund->RegisterAllActorTickFunctions(/*bRegister=*/true, /*bDoComponents=*/true);
	}

	// ------------------------------------------------------------------ der Fehlerfall
	//
	// SO ENTSTEHT ER IN ECHTEN PROJEKTEN: der Actor wird mit abgeschaltetem Tick gebaut, und
	// irgendwo spaeter setzt jemand `bCanEverTick` wieder auf true — in einer Blueprint-
	// Elternklasse, in einem Konstruktionsskript, per Datentabelle. Die Registrierung ist da
	// laengst gelaufen. Der Schalter steht dann auf „ja", und niemand tickt.
	FActorSpawnParameters Parameter;
	ATickLedgerTickProbe* Stumm = Welt->SpawnActor<ATickLedgerTickProbe>(
		ATickLedgerTickProbe::StaticClass(), FTransform::Identity, Parameter);
	TestNotNull(TEXT("der stumme Actor wurde gespawnt"), Stumm);
	if (!Gesund || !Stumm)
	{
		GEngine->DestroyWorldContext(Welt);
		Welt->DestroyWorld(false);
		return false;
	}

	// Der stumme Actor wird GENAUSO behandelt wie der gesunde — bis auf einen Schritt. Nur so
	// ist der Unterschied dem Schritt zuzurechnen und nicht dem Aufbau.
	Stumm->RegisterAllActorTickFunctions(/*bRegister=*/true, /*bDoComponents=*/true);

	// Und jetzt der eine Schritt: abmelden, den Schalter aber auf „ja" stehen lassen — genau
	// die Reihenfolge, die den Fall in echten Projekten erzeugt.
	Stumm->PrimaryActorTick.UnRegisterTickFunction();
	Stumm->PrimaryActorTick.bCanEverTick = true;

	// ------------------------------------------------------------------ Behauptung 1
	const bool bGesundRegistriert = Gesund->PrimaryActorTick.IsTickFunctionRegistered();
	const bool bStummRegistriert = Stumm->PrimaryActorTick.IsTickFunctionRegistered();

	AddInfo(FString::Printf(
		TEXT("gesund: bCanEverTick=%d registriert=%d | stumm: bCanEverTick=%d registriert=%d"),
		Gesund->PrimaryActorTick.bCanEverTick ? 1 : 0, bGesundRegistriert ? 1 : 0,
		Stumm->PrimaryActorTick.bCanEverTick ? 1 : 0, bStummRegistriert ? 1 : 0));

	TestTrue(TEXT("BEHAUPTUNG 1: bCanEverTick=true UND nicht registriert kommen zusammen vor"),
		Stumm->PrimaryActorTick.bCanEverTick && !bStummRegistriert);

	// ------------------------------------------------------------------ ticken lassen
	const int32 Runden = 10;
	for (int32 i = 0; i < Runden; ++i)
	{
		Welt->Tick(LEVELTICK_All, 1.f / 60.f);
	}

	AddInfo(FString::Printf(TEXT("nach %d Weltticks: gesund=%d Ticks, stumm=%d Ticks"),
		Runden, Gesund->TickCount, Stumm->TickCount));

	// ------------------------------------------------------------------ Behauptung 2 und 3
	//
	// DIE DRITTE IST DIE WICHTIGSTE: ohne sie wuerde der Test auch gruenen, wenn in dieser
	// Welt gar nichts tickt — und dann haette er nur sich selbst gemessen.
	TestTrue(TEXT("BEHAUPTUNG 3: der gesunde Actor tickt wirklich (sonst misst die Welt nichts)"),
		Gesund->TickCount > 0);
	TestEqual(TEXT("BEHAUPTUNG 2: der stumme Actor tickt NIE, mit eigenem Zaehler gegengeprueft"),
		Stumm->TickCount, 0);

	GEngine->DestroyWorldContext(Welt);
	Welt->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
