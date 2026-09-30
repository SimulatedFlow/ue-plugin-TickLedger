// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "TickLedgerStatics.h"

// ---------------------------------------------------------------- Enthaltung zuerst

bool UTickLedgerStatics::IsJudgeable(const FTickRecord& Record, const FTickBudget& Budget)
{
	if (Record.ObjectName.IsNone())
	{
		return false;
	}
	return Record.ObservedSeconds >= Budget.MinObservedSeconds;
}

bool UTickLedgerStatics::IsUnjudged(const FTickRecord& Record, const FTickBudget& Budget)
{
	return !IsJudgeable(Record, Budget);
}

// ------------------------------------------------- Der Fall, der KEIN Befund ist

bool UTickLedgerStatics::DoesNotWantToTick(const FTickRecord& Record)
{
	return !Record.bWantsToTick;
}

// ---------------------------------------------------------------- Die zwei Befunde

bool UTickLedgerStatics::IsWantsTickButNeverRegistered(const FTickRecord& Record)
{
	return Record.bWantsToTick && !Record.bWasEverRegistered;
}

bool UTickLedgerStatics::IsRegisteredButNeverEnabled(const FTickRecord& Record)
{
	return Record.bWasEverRegistered && !Record.bWasEverEnabled;
}

ETickFault UTickLedgerStatics::ClassifyRecord(const FTickRecord& Record,
	const FTickBudget& Budget)
{
	// 1. Enthaltung. Ohne Nenner kein „nie".
	if (IsUnjudged(Record, Budget))
	{
		return Budget.bReportUnjudged ? ETickFault::Unjudged : ETickFault::None;
	}

	// 2. „Will gar nicht ticken" als AUSSCHLUSS — vor allen Befunden. Sonst ginge jedes stille
	//    Objekt als „nie registriert" durch, und still ist die Mehrheit.
	if (DoesNotWantToTick(Record))
	{
		return Budget.bDoesNotWantToTickIsAFinding
			? ETickFault::DoesNotWantToTick : ETickFault::None;
	}

	// 3. Die Ursache vor ihrer Folge: wer nie registriert war, war auch nie eingeschaltet.
	if (IsWantsTickButNeverRegistered(Record))
	{
		return ETickFault::WantsTickButNeverRegistered;
	}

	if (IsRegisteredButNeverEnabled(Record))
	{
		return Budget.bReportRegisteredButNeverEnabled
			? ETickFault::RegisteredButNeverEnabled : ETickFault::None;
	}

	return ETickFault::None;
}

// ---------------------------------------------------------------- Zaehlen und urteilen

int32 UTickLedgerStatics::CountFaults(const TArray<FTickRecord>& Records, ETickFault Fault,
	const FTickBudget& Budget)
{
	int32 Zahl = 0;
	for (const FTickRecord& R : Records)
	{
		if (ClassifyRecord(R, Budget) == Fault)
		{
			++Zahl;
		}
	}
	return Zahl;
}

float UTickLedgerStatics::HealthyShare(const TArray<FTickRecord>& Records,
	const FTickBudget& Budget)
{
	int32 Nenner = 0;
	int32 Zaehler = 0;
	for (const FTickRecord& R : Records)
	{
		if (!IsJudgeable(R, Budget))
		{
			continue;
		}
		++Nenner;
		if (ClassifyRecord(R, Budget) == ETickFault::None)
		{
			++Zaehler;
		}
	}
	// KEIN NENNER, KEINE ZAHL. Eine 0 waere eine Zahl, die niemand gemessen hat — und sie
	// liesse sich im Bericht nicht von „alles kaputt" unterscheiden.
	if (Nenner == 0)
	{
		return -1.f;
	}
	return 100.f * static_cast<float>(Zaehler) / static_cast<float>(Nenner);
}

ETickVerdict UTickLedgerStatics::Judge(const TArray<FTickRecord>& Records,
	const FTickBudget& Budget, bool bAnyCandidateSeen)
{
	// EIN LAUF OHNE EINEN EINZIGEN TICK-KANDIDATEN IST KEIN BESTEHEN — er hat nichts geprueft.
	if (!bAnyCandidateSeen && Budget.bNoTickCandidatesIsAnError)
	{
		return ETickVerdict::Fail;
	}

	const int32 Stumm = CountFaults(Records, ETickFault::WantsTickButNeverRegistered, Budget);
	const int32 Schlafend = CountFaults(Records, ETickFault::RegisteredButNeverEnabled, Budget);
	const int32 Still = CountFaults(Records, ETickFault::DoesNotWantToTick, Budget);

	const bool bStummUeber = Stumm > Budget.MaxWantsTickButNeverRegistered;

	// EIN ABGESCHALTETER FEHLER WIRD ZUR WARNUNG, NIE ZUM SCHWEIGEN. Wer den Schalter
	// umlegt, will kein rotes Tor — er will nicht, dass der Befund verschwindet.
	if (bStummUeber && !Budget.bWantsTickButNeverRegisteredIsError)
	{
		return ETickVerdict::Warn;
	}
	if (bStummUeber)
	{
		return ETickVerdict::Fail;
	}
	if (Schlafend > Budget.MaxRegisteredButNeverEnabled)
	{
		return ETickVerdict::Warn;
	}
	// Wer `bDoesNotWantToTickIsAFinding` einschaltet, will JEDEN sehen — deshalb kein Budget:
	// ein Schwellwert wuerde genau die Liste wieder unterdruecken, um die gebeten wurde.
	if (Still > 0)
	{
		return ETickVerdict::Warn;
	}
	return ETickVerdict::Pass;
}

int32 UTickLedgerStatics::VerdictExitCode(ETickVerdict Verdict)
{
	switch (Verdict)
	{
	case ETickVerdict::Fail: return 2;
	case ETickVerdict::Warn: return 1;
	default:                 return 0;
	}
}

FString UTickLedgerStatics::VerdictText(ETickVerdict Verdict)
{
	switch (Verdict)
	{
	case ETickVerdict::Fail: return TEXT("FAIL");
	case ETickVerdict::Warn: return TEXT("WARN");
	default:                 return TEXT("PASS");
	}
}

// ---------------------------------------------------------------- Darstellung

FString UTickLedgerStatics::FaultName(ETickFault Fault)
{
	switch (Fault)
	{
	case ETickFault::WantsTickButNeverRegistered: return TEXT("NEVER REGISTERED");
	case ETickFault::RegisteredButNeverEnabled:   return TEXT("never enabled");
	case ETickFault::DoesNotWantToTick:           return TEXT("does not tick");
	case ETickFault::Unjudged:                    return TEXT("not judged");
	default:                                      return TEXT("ok");
	}
}

FString UTickLedgerStatics::FaultText(const FTickRecord& Record, const FTickBudget& Budget)
{
	switch (ClassifyRecord(Record, Budget))
	{
	case ETickFault::WantsTickButNeverRegistered:
		return FString::Printf(
			TEXT("NEVER REGISTERED - bCanEverTick is true, but the tick function was never "
				 "registered, so Tick() never ran. There is no error and nothing in the log. "
				 "This happens when the flag is switched on AFTER registration already went "
				 "past%s"),
			Record.bIsComponent
				? TEXT(" - on a component, which is the easier half to overlook")
				: TEXT(""));

	case ETickFault::RegisteredButNeverEnabled:
		return TEXT("never enabled - registered and ready, but never switched on in this run. "
					"A warning, not an error: an actor that waits for its cue looks exactly "
					"like one whose cue never came");

	case ETickFault::DoesNotWantToTick:
		return TEXT("does not tick - bCanEverTick is false. Listed because "
					"bDoesNotWantToTickIsAFinding is on; this is the recommended default and "
					"never an error");

	case ETickFault::Unjudged:
		return FString::Printf(
			TEXT("not judged - watched for %.1f s, the floor is %.1f s"),
			Record.ObservedSeconds, Budget.MinObservedSeconds);

	default:
		if (DoesNotWantToTick(Record))
		{
			// KEIN BEFUND, UND DER BERICHT SAGT DAS AUCH. Wer hier nichts schreibt, laesst den
			// Leser raten, ob das Objekt uebersehen wurde.
			return TEXT("ok - does not want to tick, which is the recommended default");
		}
		return TEXT("ok");
	}
}

FString UTickLedgerStatics::RecordHeader()
{
	return FString::Printf(TEXT("%-34s %-22s %-5s %-6s %-6s  %s"),
		TEXT("object"), TEXT("owner"), TEXT("wants"), TEXT("regd"), TEXT("enabl"),
		TEXT("note"));
}

FString UTickLedgerStatics::RecordLine(const FTickRecord& Record, const FTickBudget& Budget)
{
	return FString::Printf(TEXT("%-34s %-22s %-5s %-6s %-6s  %s"),
		*Record.ObjectName.ToString(),
		Record.OwnerName.IsNone() ? TEXT("-") : *Record.OwnerName.ToString(),
		Record.bWantsToTick ? TEXT("yes") : TEXT("no"),
		Record.bWasEverRegistered ? TEXT("yes") : TEXT("NO"),
		Record.bWasEverEnabled ? TEXT("yes") : TEXT("no"),
		*FaultText(Record, Budget));
}

FLinearColor UTickLedgerStatics::VerdictColour(ETickVerdict Verdict)
{
	switch (Verdict)
	{
	case ETickVerdict::Fail: return FLinearColor(0.95f, 0.25f, 0.25f);
	case ETickVerdict::Warn: return FLinearColor(0.95f, 0.80f, 0.20f);
	default:                 return FLinearColor(0.30f, 0.85f, 0.40f);
	}
}

FLinearColor UTickLedgerStatics::FaultColour(ETickFault Fault)
{
	switch (Fault)
	{
	case ETickFault::WantsTickButNeverRegistered: return FLinearColor(0.95f, 0.25f, 0.25f);
	case ETickFault::RegisteredButNeverEnabled:   return FLinearColor(0.95f, 0.80f, 0.20f);
	case ETickFault::DoesNotWantToTick:           return FLinearColor(0.55f, 0.75f, 0.95f);
	case ETickFault::Unjudged:                    return FLinearColor(0.65f, 0.65f, 0.70f);
	default:                                      return FLinearColor(0.75f, 0.78f, 0.82f);
	}
}

FString UTickLedgerStatics::ExplainLimits(const FTickBudget& Budget)
{
	TArray<FString> Zeilen;
	Zeilen.Add(TEXT("1. It records what each object was in THIS run. An actor that never "
					"spawned is not in the report at all - this tool watches what ran, it does "
					"not search your project."));
	Zeilen.Add(FString::Printf(
		TEXT("2. An object watched for less than %.2f s is not judged. Without that floor a "
			 "short run reports every late spawn as broken - correctly computed and completely "
			 "worthless."),
		Budget.MinObservedSeconds));
	Zeilen.Add(TEXT("3. bCanEverTick = false is NOT a finding. Most objects in a scene do not "
					"tick, and that is the recommended default."));
	Zeilen.Add(TEXT("4. 'never enabled' is a warning, never an error: an actor waiting for its "
					"cue is indistinguishable from one whose cue never came."));
	Zeilen.Add(TEXT("5. It keeps what each object EVER was, not the state at the end. Something "
					"that ticked for a minute and was switched off did its job."));
	Zeilen.Add(FString::Printf(
		TEXT("6. It tracks at most %d objects%s; when the cap is hit the report says so."),
		Budget.MaxTrackedObjects,
		Budget.bIncludeComponents ? TEXT(", actors and components") : TEXT(", actors only")));
	Zeilen.Add(TEXT("7. It is not a profiler: registration and enable state, never milliseconds "
					"and never tick cost."));
	Zeilen.Add(TEXT("8. It changes nothing - no tick function is registered, enabled or "
					"disabled."));
	return FString::Join(Zeilen, TEXT("\n"));
}
