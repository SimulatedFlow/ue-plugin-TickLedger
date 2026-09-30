// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"

#include "TickLedgerLog.h"
#include "TickLedgerSettings.h"
#include "TickLedgerStatics.h"
#include "TickLedgerSubsystem.h"

namespace
{
	UTickLedgerSubsystem* Ledger(UWorld* W)
	{
		UTickLedgerSubsystem* S = W ? W->GetSubsystem<UTickLedgerSubsystem>() : nullptr;
		if (!S)
		{
			UE_LOG(LogTickLedger, Warning,
				TEXT("TickLedger: no game world - this only runs while a game world is up."));
		}
		return S;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdShow(
		TEXT("TickLedger.Show"),
		TEXT("TickLedger.Show - draw the panel."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>&, UWorld*)
			{
				GetMutableDefault<UTickLedgerSettings>()->bShowOverlay = true;
			}));

	FAutoConsoleCommandWithWorldAndArgs CmdHide(
		TEXT("TickLedger.Hide"),
		TEXT("TickLedger.Hide - hide the panel."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>&, UWorld*)
			{
				GetMutableDefault<UTickLedgerSettings>()->bShowOverlay = false;
			}));

	FAutoConsoleCommandWithWorldAndArgs CmdReset(
		TEXT("TickLedger.Reset"),
		TEXT("TickLedger.Reset - forget everything and start the run over."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>&, UWorld* W)
			{
				if (UTickLedgerSubsystem* S = Ledger(W))
				{
					S->ResetLedger();
					UE_LOG(LogTickLedger, Display, TEXT("TickLedger: run reset."));
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs CmdDump(
		TEXT("TickLedger.Dump"),
		TEXT("TickLedger.Dump - print every object with a finding to the log."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>&, UWorld* W)
			{
				UTickLedgerSubsystem* S = Ledger(W);
				if (!S)
				{
					return;
				}
				const FTickBudget Budget = S->GetBudget();
				UE_LOG(LogTickLedger, Display, TEXT("%s"), *S->SummaryLine());
				UE_LOG(LogTickLedger, Display, TEXT("%s"), *UTickLedgerStatics::RecordHeader());

				// NUR DIE MIT BEFUND — und zwar aus demselben Grund, aus dem der Kasten sie
				// filtert: in einer gewoehnlichen Szene sind die meisten Objekte still, und
				// eine Liste mit fuenfhundert unauffaelligen Zeilen verbirgt die drei, um die
				// es geht. Alles steht im Bericht.
				const TArray<FTickRecord> MitBefund = S->GetFindingsBySeverity();
				int32 Gezeigt = 0;
				for (const FTickRecord& R : MitBefund)
				{
					const ETickFault Art = UTickLedgerStatics::ClassifyRecord(R, Budget);
					if (Art == ETickFault::Unjudged || Art == ETickFault::DoesNotWantToTick)
					{
						continue;
					}
					++Gezeigt;
					UE_LOG(LogTickLedger, Display, TEXT("%s"),
						*UTickLedgerStatics::RecordLine(R, Budget));
				}
				if (Gezeigt == 0)
				{
					// KEINE ZEILE IST KEINE GUTE NACHRICHT, solange nicht dasteht, WARUM.
					UE_LOG(LogTickLedger, Display,
						TEXT("   (no findings - every object that wanted to tick was "
							 "registered for it)"));
				}
			}));

	/**
	 * Die Befunde nach KLASSE gruppieren — der Befehl, der beim Suchen wirklich hilft.
	 *
	 * Bei diesem Werkzeug ist der Einzelname selten die Antwort: heissen dreissig Eintraege
	 * `BP_Turret_C_0` bis `_29`, ist das EINE Ursache und nicht dreissig. Die Klasse zeigt
	 * das, die Namensliste verbirgt es.
	 */
	FAutoConsoleCommandWithWorldAndArgs CmdClasses(
		TEXT("TickLedger.Classes"),
		TEXT("TickLedger.Classes - group the findings by class; thirty objects of one class "
			 "are one cause, not thirty."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>&, UWorld* W)
			{
				UTickLedgerSubsystem* S = Ledger(W);
				if (!S)
				{
					return;
				}
				const FTickBudget Budget = S->GetBudget();
				TMap<FName, int32> ProKlasse;
				for (const FTickRecord& R : S->GetRecords())
				{
					const ETickFault Art = UTickLedgerStatics::ClassifyRecord(R, Budget);
					if (Art != ETickFault::WantsTickButNeverRegistered
						&& Art != ETickFault::RegisteredButNeverEnabled)
					{
						continue;
					}
					++ProKlasse.FindOrAdd(R.ClassName);
				}
				if (ProKlasse.Num() == 0)
				{
					UE_LOG(LogTickLedger, Display,
						TEXT("   (no class has a finding)"));
					return;
				}
				ProKlasse.ValueSort([](int32 A, int32 B) { return A > B; });
				for (const TPair<FName, int32>& Paar : ProKlasse)
				{
					UE_LOG(LogTickLedger, Display, TEXT("%-40s %d object(s) with a finding"),
						*Paar.Key.ToString(), Paar.Value);
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs CmdReport(
		TEXT("TickLedger.Report"),
		TEXT("TickLedger.Report [path] - write the report as JSON "
			 "(default: Saved/TickLedger/report.json)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* W)
			{
				if (UTickLedgerSubsystem* S = Ledger(W))
				{
					if (S->WriteReport(Args.Num() > 0 ? Args[0] : TEXT("report")).IsEmpty())
					{
						UE_LOG(LogTickLedger, Warning,
							TEXT("TickLedger.Report: nothing was written."));
					}
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs CmdGate(
		TEXT("TickLedger.Gate"),
		TEXT("TickLedger.Gate <seconds> [-noexit] - measure, write the report, "
			 "exit 0 clean / 1 warnings / 2 errors."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* W)
			{
				UTickLedgerSubsystem* S = W ? W->GetSubsystem<UTickLedgerSubsystem>() : nullptr;
				if (!S)
				{
					// EIN LAUF OHNE WELT IST KEIN BESTEHEN.
					UE_LOG(LogTickLedger, Error,
						TEXT("TickLedger.Gate: no game world, so nothing can be measured. "
							 "That is not a pass."));
					FPlatformMisc::RequestExitWithStatus(/*Force=*/true, 2);
					return;
				}

				float Sekunden = 60.f;
				bool bExit = true;
				for (const FString& A : Args)
				{
					if (A.Equals(TEXT("-noexit"), ESearchCase::IgnoreCase))
					{
						bExit = false;
					}
					else if (A.IsNumeric())
					{
						Sekunden = FCString::Atof(*A);
					}
				}

				// EIN HINWEIS, DER GELD SPART: wer das Tor kuerzer laufen laesst als die
				// Beobachtungsschwelle, bekommt lauter Enthaltungen und wundert sich ueber
				// einen gruenen Lauf, der nichts geprueft hat.
				const float Mindest = S->GetBudget().MinObservedSeconds;
				if (Sekunden > 0.f && Sekunden < Mindest)
				{
					UE_LOG(LogTickLedger, Warning,
						TEXT("TickLedger.Gate: %.2f s is shorter than MinObservedSeconds "
							 "(%.2f s), so every object will be 'not judged' - a green run "
							 "that proves nothing."),
						Sekunden, Mindest);
				}

				S->StartGate(Sekunden, bExit);
			}));
}
