// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "TickLedgerStatics.h"
#include "TickLedgerTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TickLedgerTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter;

	/** Ein gesunder Eintrag: will ticken, war registriert und eingeschaltet. */
	FTickRecord Gesund()
	{
		FTickRecord R;
		R.ObjectName = TEXT("BP_Turret_C_0");
		R.ClassName = TEXT("BP_Turret_C");
		R.bWantsToTick = true;
		R.bWasEverRegistered = true;
		R.bWasEverEnabled = true;
		R.ObservedSeconds = 10.f;
		return R;
	}

	/** Ein stiller Eintrag: will gar nicht ticken — der Normalfall in jeder Szene. */
	FTickRecord Still()
	{
		FTickRecord R = Gesund();
		R.ObjectName = TEXT("StaticMeshActor_7");
		R.ClassName = TEXT("StaticMeshActor");
		R.bWantsToTick = false;
		R.bWasEverRegistered = false;
		R.bWasEverEnabled = false;
		return R;
	}
}

// ---------------------------------------------------------------------------------------
// 1. STILL IST KEIN BEFUND.
//
// Der Test, der dieses Plugin von einer naiven Fassung unterscheidet: in einer gewoehnlichen
// Szene tickt die MEHRHEIT der Objekte nicht, und das ist die empfohlene Bauweise.
// ---------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTickLedgerSilentTest,
	"TickLedger.Rules.SilentIsNotAFinding",
	TickLedgerTests::TestFlags)

bool FTickLedgerSilentTest::RunTest(const FString&)
{
	using namespace TickLedgerTests;
	const FTickBudget Budget;

	const FTickRecord S = Still();
	TestTrue(TEXT("will gar nicht ticken"), UTickLedgerStatics::DoesNotWantToTick(S));
	TestEqual(TEXT("und ist damit KEIN Befund"),
		UTickLedgerStatics::ClassifyRecord(S, Budget), ETickFault::None);

	// GEGENPROBE: der gesunde Eintrag ist NICHT still. Ohne sie wuerde `DoesNotWantToTick`
	// auch dann gruenen, wenn es schlicht immer true lieferte.
	TestFalse(TEXT("der gesunde Eintrag will sehr wohl ticken"),
		UTickLedgerStatics::DoesNotWantToTick(Gesund()));

	// Und wer es doch sehen will, bekommt es — aber nur ausdruecklich und nur als Warnung.
	FTickBudget Streng;
	Streng.bDoesNotWantToTickIsAFinding = true;
	TestEqual(TEXT("mit dem Schalter wird daraus eine eigene Meldung"),
		UTickLedgerStatics::ClassifyRecord(S, Streng), ETickFault::DoesNotWantToTick);
	return true;
}

// ---------------------------------------------------------------------------------------
// 2. DIE ENTHALTUNG STEHT VOR ALLEM ANDEREN.
// ---------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTickLedgerAbstentionTest,
	"TickLedger.Rules.AbstentionComesFirst",
	TickLedgerTests::TestFlags)

bool FTickLedgerAbstentionTest::RunTest(const FString&)
{
	using namespace TickLedgerTests;
	const FTickBudget Budget;

	FTickRecord Kurz = Gesund();
	Kurz.bWasEverRegistered = false;     // waere ein Fehler …
	Kurz.bWasEverEnabled = false;
	Kurz.ObservedSeconds = 2.f;          // … aber zu kurz gesehen

	TestFalse(TEXT("zwei Sekunden reichen nicht"),
		UTickLedgerStatics::IsJudgeable(Kurz, Budget));
	TestEqual(TEXT("und werden zur Enthaltung, NICHT zum Fehler"),
		UTickLedgerStatics::ClassifyRecord(Kurz, Budget), ETickFault::Unjudged);

	// Derselbe Eintrag, lange genug gesehen: JETZT ist es ein Fehler. Ohne diese Haelfte
	// wuerde der Test auch gruenen, wenn die Klassifizierung immer Unjudged saegte.
	Kurz.ObservedSeconds = 10.f;
	TestEqual(TEXT("lange genug beobachtet wird derselbe Eintrag zum Fehler"),
		UTickLedgerStatics::ClassifyRecord(Kurz, Budget),
		ETickFault::WantsTickButNeverRegistered);

	FTickRecord Huelse = Gesund();
	Huelse.ObjectName = NAME_None;
	TestFalse(TEXT("ohne Namen nicht beurteilbar"),
		UTickLedgerStatics::IsJudgeable(Huelse, Budget));
	return true;
}

// ---------------------------------------------------------------------------------------
// 3. DIE URSACHE GEWINNT GEGEN IHRE FOLGE.
//
// Wer nie registriert war, war auch nie eingeschaltet — beide treffen zu, aber nur die erste
// nennt den Grund.
// ---------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTickLedgerCauseTest,
	"TickLedger.Rules.CauseBeatsEffect",
	TickLedgerTests::TestFlags)

bool FTickLedgerCauseTest::RunTest(const FString&)
{
	using namespace TickLedgerTests;
	const FTickBudget Budget;

	FTickRecord Stumm = Gesund();
	Stumm.bWasEverRegistered = false;
	Stumm.bWasEverEnabled = false;

	TestTrue(TEXT("will ticken, war nie registriert"),
		UTickLedgerStatics::IsWantsTickButNeverRegistered(Stumm));
	TestEqual(TEXT("gemeldet wird die URSACHE"),
		UTickLedgerStatics::ClassifyRecord(Stumm, Budget),
		ETickFault::WantsTickButNeverRegistered);

	// `IsRegisteredButNeverEnabled` darf hier gar nicht zuschlagen — sonst waere „nie
	// eingeschaltet" nur durch die Reihenfolge verdeckt statt sachlich ausgeschlossen.
	TestFalse(TEXT("'nie eingeschaltet' trifft ohne Registrierung gar nicht zu"),
		UTickLedgerStatics::IsRegisteredButNeverEnabled(Stumm));

	// Und der andere Fall bleibt uebrig, wenn die Registrierung da war.
	FTickRecord Schlaeft = Gesund();
	Schlaeft.bWasEverEnabled = false;
	TestEqual(TEXT("registriert, nie eingeschaltet bleibt als eigener Befund"),
		UTickLedgerStatics::ClassifyRecord(Schlaeft, Budget),
		ETickFault::RegisteredButNeverEnabled);
	return true;
}

// ---------------------------------------------------------------------------------------
// 4. „JEMALS", NICHT „AM ENDE".
//
// Ein Objekt, das eine Minute getickt hat und dann abgeschaltet wurde, hat seine Arbeit
// getan. Die Regeln duerfen es nicht als nie gelaufen melden.
// ---------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTickLedgerEverTest,
	"TickLedger.Rules.EverNotFinal",
	TickLedgerTests::TestFlags)

bool FTickLedgerEverTest::RunTest(const FString&)
{
	using namespace TickLedgerTests;
	const FTickBudget Budget;

	// So sieht ein ordentlich beendetes Objekt im Ledger aus: es WAR registriert und WAR
	// eingeschaltet — auch wenn beides am Ende nicht mehr gilt.
	const FTickRecord Beendet = Gesund();
	TestEqual(TEXT("was je registriert und je eingeschaltet war, ist kein Befund"),
		UTickLedgerStatics::ClassifyRecord(Beendet, Budget), ETickFault::None);

	// Und die Gegenprobe: waere hier der Endzustand gespeichert, saehe es so aus — und das
	// MUSS ein anderer Befund sein.
	FTickRecord AlsWaereEsDerEndzustand = Gesund();
	AlsWaereEsDerEndzustand.bWasEverRegistered = false;
	AlsWaereEsDerEndzustand.bWasEverEnabled = false;
	TestNotEqual(TEXT("mit dem Endzustand statt dem Hoechststand waere es ein Fehler"),
		UTickLedgerStatics::ClassifyRecord(AlsWaereEsDerEndzustand, Budget), ETickFault::None);
	return true;
}

// ---------------------------------------------------------------------------------------
// 5. EHRLICHE ZAHLEN.
// ---------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTickLedgerHonestTest,
	"TickLedger.Rules.HonestNumbers",
	TickLedgerTests::TestFlags)

bool FTickLedgerHonestTest::RunTest(const FString&)
{
	using namespace TickLedgerTests;
	const FTickBudget Budget;

	TArray<FTickRecord> Leer;
	TestTrue(TEXT("ohne Eintraege gibt es keine Quote, sondern -1"),
		UTickLedgerStatics::HealthyShare(Leer, Budget) < 0.f);

	FTickRecord Kurz = Gesund();
	Kurz.ObservedSeconds = 1.f;
	TArray<FTickRecord> NurEnthaltung = { Kurz };
	TestTrue(TEXT("nur Enthaltungen ergeben ebenfalls keine Quote"),
		UTickLedgerStatics::HealthyShare(NurEnthaltung, Budget) < 0.f);

	FTickRecord Kaputt = Gesund();
	Kaputt.bWasEverRegistered = false;
	Kaputt.bWasEverEnabled = false;
	TArray<FTickRecord> Zwei = { Gesund(), Kaputt };
	TestEqual(TEXT("ein gesunder von zweien sind 50 Prozent"),
		UTickLedgerStatics::HealthyShare(Zwei, Budget), 50.f);

	// STILLE OBJEKTE ZAEHLEN ALS GESUND — und das ist wichtig: in einer echten Szene sind es
	// hunderte, und wuerden sie die Quote druecken, staende dort immer eine schlechte Zahl.
	TArray<FTickRecord> MitStillen = { Gesund(), Still(), Still() };
	TestEqual(TEXT("stille Objekte druecken die Quote nicht"),
		UTickLedgerStatics::HealthyShare(MitStillen, Budget), 100.f);
	return true;
}

// ---------------------------------------------------------------------------------------
// 6. DAS URTEIL.
// ---------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTickLedgerVerdictTest,
	"TickLedger.Rules.Verdict",
	TickLedgerTests::TestFlags)

bool FTickLedgerVerdictTest::RunTest(const FString&)
{
	using namespace TickLedgerTests;
	const FTickBudget Budget;

	TArray<FTickRecord> Gut = { Gesund() };
	TestEqual(TEXT("ein gesundes Objekt besteht"),
		UTickLedgerStatics::Judge(Gut, Budget, true), ETickVerdict::Pass);

	// EIN LAUF OHNE EINEN EINZIGEN KANDIDATEN IST KEIN BESTEHEN.
	TArray<FTickRecord> Nichts;
	TestEqual(TEXT("kein Tick-Kandidat gesehen ist ein Fehler, kein gruenes Nichts"),
		UTickLedgerStatics::Judge(Nichts, Budget, false), ETickVerdict::Fail);

	FTickRecord Stumm = Gesund();
	Stumm.bWasEverRegistered = false;
	Stumm.bWasEverEnabled = false;
	TArray<FTickRecord> MitFehler = { Stumm };
	TestEqual(TEXT("will ticken, nie registriert ist ein Fehler"),
		UTickLedgerStatics::Judge(MitFehler, Budget, true), ETickVerdict::Fail);

	// ABGESCHALTET HEISST WARNUNG, NICHT SCHWEIGEN.
	FTickBudget Weich;
	Weich.bWantsTickButNeverRegisteredIsError = false;
	TestEqual(TEXT("abgeschaltet wird daraus eine Warnung"),
		UTickLedgerStatics::Judge(MitFehler, Weich, true), ETickVerdict::Warn);

	TestEqual(TEXT("PASS -> 0"), UTickLedgerStatics::VerdictExitCode(ETickVerdict::Pass), 0);
	TestEqual(TEXT("WARN -> 1"), UTickLedgerStatics::VerdictExitCode(ETickVerdict::Warn), 1);
	TestEqual(TEXT("FAIL -> 2"), UTickLedgerStatics::VerdictExitCode(ETickVerdict::Fail), 2);
	return true;
}

// ---------------------------------------------------------------------------------------
// 7. DIE NOTIZ SAGT, WO MAN SUCHEN MUSS.
// ---------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTickLedgerNoteTest,
	"TickLedger.Report.NoteHelpsTheReader",
	TickLedgerTests::TestFlags)

bool FTickLedgerNoteTest::RunTest(const FString&)
{
	using namespace TickLedgerTests;
	const FTickBudget Budget;

	FTickRecord Stumm = Gesund();
	Stumm.bWasEverRegistered = false;
	Stumm.bWasEverEnabled = false;
	const FString Notiz = UTickLedgerStatics::FaultText(Stumm, Budget);
	TestTrue(TEXT("die Notiz nennt den Schalter"), Notiz.Contains(TEXT("bCanEverTick")));
	TestTrue(TEXT("und sagt, wann es passiert"), Notiz.Contains(TEXT("AFTER registration")));

	// BEI EINER KOMPONENTE STEHT DAS AUCH DA — sie ist die Haelfte, die man uebersieht.
	FTickRecord AlsKomponente = Stumm;
	AlsKomponente.bIsComponent = true;
	const FString KompNotiz = UTickLedgerStatics::FaultText(AlsKomponente, Budget);
	TestTrue(TEXT("bei einer Komponente sagt die Notiz das ausdruecklich"),
		KompNotiz.Contains(TEXT("component")));
	TestNotEqual(TEXT("und unterscheidet sich damit von der Actor-Notiz"), KompNotiz, Notiz);

	// Und beim stillen Objekt steht, dass das die empfohlene Voreinstellung ist.
	const FString StillNotiz = UTickLedgerStatics::FaultText(Still(), Budget);
	TestTrue(TEXT("das stille Objekt wird als Normalfall ausgewiesen"),
		StillNotiz.Contains(TEXT("recommended default")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
