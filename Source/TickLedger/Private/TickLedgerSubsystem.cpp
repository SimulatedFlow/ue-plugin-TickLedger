// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "TickLedgerSubsystem.h"

#include "TickLedgerLog.h"
#include "TickLedgerSettings.h"
#include "TickLedgerStatics.h"

// DIESE INCLUDES SIND KEINE ZIERDE. Bei JointLedger fehlten `Engine/Engine.h` und die
// JSON-Policy, und der Bau OHNE Unity endete mit 102 Fehlern — im Unity-Block hatte sie ein
// Nachbar mitgebracht. Hier stehen sie von Anfang an.
#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UTickLedgerSubsystem* UTickLedgerSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}
	const UWorld* Welt = GEngine
		? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return Welt ? Welt->GetSubsystem<UTickLedgerSubsystem>() : nullptr;
}

void UTickLedgerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const UTickLedgerSettings* S = UTickLedgerSettings::Get();
	UE_LOG(LogTickLedger, Log, TEXT("TickLedger watching at %.1f Hz%s"),
		S ? S->SampleHz : 4.f,
		(S && S->Budget.bIncludeComponents) ? TEXT(" (actors and components)")
											: TEXT(" (actors only)"));
}

bool UTickLedgerSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UTickLedgerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTickLedgerSubsystem, STATGROUP_Tickables);
}

void UTickLedgerSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ElapsedSeconds += DeltaTime;
	SinceSample += DeltaTime;

	const UTickLedgerSettings* S = UTickLedgerSettings::Get();
	const float Hz = (S && S->SampleHz > 0.f) ? S->SampleHz : 4.f;
	const float Takt = 1.f / Hz;

	if (SinceSample >= Takt)
	{
		// DIE LAENGSTE LUECKE GEHOERT IN DEN BERICHT. Eine Messung, die ihre eigene Guete
		// verschweigt, moechte geglaubt und nicht geprueft werden.
		LongestGapSeconds = FMath::Max(LongestGapSeconds, SinceSample);
		SinceSample = 0.f;
		SampleWorld();
	}

	if (bGateRunning)
	{
		GateElapsed += DeltaTime;
		if (GateElapsed >= GateSeconds)
		{
			FinishGate();
		}
	}
}

void UTickLedgerSubsystem::ResetLedger()
{
	Records.Reset();
	Index.Reset();
	SinceSample = 0.f;
	ElapsedSeconds = 0.f;
	SamplesTaken = 0;
	LongestGapSeconds = 0.f;
	bAnyCandidateSeen = false;
	bHitCap = false;
}

void UTickLedgerSubsystem::SampleNow()
{
	SampleWorld();
}

FTickRecord* UTickLedgerSubsystem::FindOrAdd(FName ObjectName, FName OwnerName,
	FName ClassName, bool bIsComponent)
{
	// DER SCHLUESSEL TRAEGT BEIDES. Ein Actor haengt Dutzende Komponenten an sich, und
	// „SceneComponent_0" heisst auf dreissig Actors gleich; ohne den Besitzer verschmelzen
	// sie zu einem Eintrag und ein Befund verschwindet hinter einem gesunden Nachbarn.
	const FString Schluessel = FString::Printf(TEXT("%s|%s"),
		OwnerName.IsNone() ? TEXT("-") : *OwnerName.ToString(), *ObjectName.ToString());

	if (const int32* Platz = Index.Find(Schluessel))
	{
		return &Records[*Platz];
	}

	const UTickLedgerSettings* S = UTickLedgerSettings::Get();
	const int32 Kappung = S ? S->Budget.MaxTrackedObjects : 512;
	if (Records.Num() >= Kappung)
	{
		bHitCap = true;
		return nullptr;
	}

	FTickRecord Neu;
	Neu.ObjectName = ObjectName;
	Neu.OwnerName = OwnerName;
	Neu.ClassName = ClassName;
	Neu.bIsComponent = bIsComponent;
	const int32 Platz = Records.Add(Neu);
	Index.Add(Schluessel, Platz);
	return &Records[Platz];
}

void UTickLedgerSubsystem::Fortschreiben(FTickRecord& R, bool bCanEverTick, bool bRegistered,
	bool bEnabled, float Takt)
{
	R.bWantsToTick |= bCanEverTick;
	R.bWasEverRegistered |= bRegistered;
	R.bWasEverEnabled |= bEnabled;
	R.ObservedSeconds += Takt;

	if (bCanEverTick)
	{
		// „Kandidat" ist, wer ticken WILL. Ein Lauf ohne einen einzigen davon hat nichts
		// geprueft — und das ist nicht dasselbe wie „alles in Ordnung".
		bAnyCandidateSeen = true;
	}
}

void UTickLedgerSubsystem::SampleWorld()
{
	UWorld* Welt = GetWorld();
	if (!Welt)
	{
		return;
	}

	const UTickLedgerSettings* S = UTickLedgerSettings::Get();
	const FTickBudget Budget = S ? S->Budget : FTickBudget();
	const float Takt = (S && S->SampleHz > 0.f) ? (1.f / S->SampleHz) : 0.25f;

	++SamplesTaken;

	for (TActorIterator<AActor> It(Welt); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}

		if (FTickRecord* R = FindOrAdd(Actor->GetFName(), NAME_None,
			Actor->GetClass() ? Actor->GetClass()->GetFName() : NAME_None, /*bIsComponent=*/false))
		{
			Fortschreiben(*R, Actor->PrimaryActorTick.bCanEverTick,
				Actor->PrimaryActorTick.IsTickFunctionRegistered(),
				Actor->PrimaryActorTick.IsTickFunctionEnabled(), Takt);
			R->Fault = UTickLedgerStatics::ClassifyRecord(*R, Budget);
		}

		if (!Budget.bIncludeComponents)
		{
			continue;
		}

		// DIE KOMPONENTEN SIND DIE UNAUFFAELLIGERE HAELFTE. `PrimaryComponentTick` hat
		// dieselbe Struktur und dieselbe Falle; ein Werkzeug, das nur Actors ansieht, findet
		// den haeufigeren Fall nicht.
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (!IsValid(Comp))
			{
				continue;
			}
			FTickRecord* RC = FindOrAdd(Comp->GetFName(), Actor->GetFName(),
				Comp->GetClass() ? Comp->GetClass()->GetFName() : NAME_None,
				/*bIsComponent=*/true);
			if (!RC)
			{
				continue;
			}
			Fortschreiben(*RC, Comp->PrimaryComponentTick.bCanEverTick,
				Comp->PrimaryComponentTick.IsTickFunctionRegistered(),
				Comp->PrimaryComponentTick.IsTickFunctionEnabled(), Takt);
			RC->Fault = UTickLedgerStatics::ClassifyRecord(*RC, Budget);
		}
	}
}

FTickBudget UTickLedgerSubsystem::GetBudget() const
{
	const UTickLedgerSettings* S = UTickLedgerSettings::Get();
	return S ? S->Budget : FTickBudget();
}

TArray<FTickRecord> UTickLedgerSubsystem::GetFindingsBySeverity() const
{
	const FTickBudget Budget = GetBudget();
	const ETickFault Reihenfolge[] = {
		ETickFault::WantsTickButNeverRegistered,
		ETickFault::RegisteredButNeverEnabled,
		ETickFault::DoesNotWantToTick,
		ETickFault::Unjudged };

	TArray<FTickRecord> Aus;
	for (const ETickFault Art : Reihenfolge)
	{
		for (const FTickRecord& R : Records)
		{
			if (UTickLedgerStatics::ClassifyRecord(R, Budget) == Art)
			{
				Aus.Add(R);
			}
		}
	}
	return Aus;
}

ETickVerdict UTickLedgerSubsystem::GetVerdict() const
{
	return UTickLedgerStatics::Judge(Records, GetBudget(), bAnyCandidateSeen);
}

FString UTickLedgerSubsystem::SummaryLine() const
{
	const UTickLedgerSettings* S = UTickLedgerSettings::Get();
	const FTickBudget Budget = GetBudget();
	const float Anteil = UTickLedgerStatics::HealthyShare(Records, Budget);
	const float Erreicht = (ElapsedSeconds > 0.f)
		? static_cast<float>(SamplesTaken) / ElapsedSeconds : 0.f;

	return FString::Printf(
		TEXT("TickLedger %s | %d object(s) | %d sample(s) @ %.1f Hz (%.1f wanted) in %.1f s "
			 "| healthy %s | %d never registered, %d never enabled, %d silent, %d not judged%s"),
		*UTickLedgerStatics::VerdictText(GetVerdict()),
		Records.Num(), SamplesTaken, Erreicht, S ? S->SampleHz : 4.f, ElapsedSeconds,
		// KEIN NENNER, KEINE ZAHL — `n/a` und nicht 0 %.
		(Anteil < 0.f) ? TEXT("n/a") : *FString::Printf(TEXT("%.0f%%"), Anteil),
		UTickLedgerStatics::CountFaults(Records, ETickFault::WantsTickButNeverRegistered, Budget),
		UTickLedgerStatics::CountFaults(Records, ETickFault::RegisteredButNeverEnabled, Budget),
		UTickLedgerStatics::CountFaults(Records, ETickFault::DoesNotWantToTick, Budget),
		UTickLedgerStatics::CountFaults(Records, ETickFault::Unjudged, Budget),
		bHitCap ? TEXT(" | TRACKING CAP HIT, counts are incomplete") : TEXT(""));
}

FString UTickLedgerSubsystem::DominantLine() const
{
	const FTickBudget Budget = GetBudget();
	for (const FTickRecord& R : GetFindingsBySeverity())
	{
		const ETickFault Art = UTickLedgerStatics::ClassifyRecord(R, Budget);
		// Enthaltung und „will nicht ticken" sind keine Befunde und duerfen die Kopfzeile
		// nicht besetzen.
		if (Art != ETickFault::Unjudged && Art != ETickFault::DoesNotWantToTick)
		{
			return UTickLedgerStatics::RecordLine(R, Budget);
		}
	}
	return FString();
}

FString UTickLedgerSubsystem::LimitsLine() const
{
	return UTickLedgerStatics::ExplainLimits(GetBudget());
}

FString UTickLedgerSubsystem::WriteReport(const FString& FileName) const
{
	const UTickLedgerSettings* S = UTickLedgerSettings::Get();
	const FTickBudget Budget = GetBudget();
	const float Anteil = UTickLedgerStatics::HealthyShare(Records, Budget);

	FString Aus;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Aus);

	W->WriteObjectStart();
	W->WriteValue(TEXT("tool"), TEXT("TickLedger"));
	W->WriteValue(TEXT("verdict"), UTickLedgerStatics::VerdictText(GetVerdict()));
	W->WriteValue(TEXT("exitCode"), UTickLedgerStatics::VerdictExitCode(GetVerdict()));
	W->WriteValue(TEXT("summary"), SummaryLine());
	W->WriteValue(TEXT("objectsSeen"), Records.Num());
	W->WriteValue(TEXT("samplesTaken"), SamplesTaken);
	W->WriteValue(TEXT("wantedHz"), S ? S->SampleHz : 4.f);
	W->WriteValue(TEXT("achievedHz"),
		(ElapsedSeconds > 0.f) ? static_cast<float>(SamplesTaken) / ElapsedSeconds : 0.f);
	W->WriteValue(TEXT("measuredSeconds"), ElapsedSeconds);
	W->WriteValue(TEXT("longestGapSeconds"), LongestGapSeconds);
	W->WriteValue(TEXT("anyCandidateSeen"), bAnyCandidateSeen);
	W->WriteValue(TEXT("hitTrackingCap"), bHitCap);
	W->WriteValue(TEXT("includedComponents"), Budget.bIncludeComponents);

	// NULL STATT NULL-PROZENT: ohne Nenner gibt es keine Quote.
	if (Anteil < 0.f)
	{
		W->WriteNull(TEXT("healthySharePercent"));
	}
	else
	{
		W->WriteValue(TEXT("healthySharePercent"), Anteil);
	}

	W->WriteValue(TEXT("neverRegistered"),
		UTickLedgerStatics::CountFaults(Records, ETickFault::WantsTickButNeverRegistered, Budget));
	W->WriteValue(TEXT("neverEnabled"),
		UTickLedgerStatics::CountFaults(Records, ETickFault::RegisteredButNeverEnabled, Budget));
	W->WriteValue(TEXT("doesNotWantToTick"),
		UTickLedgerStatics::CountFaults(Records, ETickFault::DoesNotWantToTick, Budget));
	W->WriteValue(TEXT("notJudged"),
		UTickLedgerStatics::CountFaults(Records, ETickFault::Unjudged, Budget));
	W->WriteValue(TEXT("limitsText"), LimitsLine());

	W->WriteArrayStart(TEXT("records"));
	for (const FTickRecord& R : Records)
	{
		W->WriteObjectStart();
		W->WriteValue(TEXT("object"), R.ObjectName.ToString());
		W->WriteValue(TEXT("owner"), R.OwnerName.ToString());
		W->WriteValue(TEXT("class"), R.ClassName.ToString());
		W->WriteValue(TEXT("isComponent"), R.bIsComponent);
		W->WriteValue(TEXT("wantsToTick"), R.bWantsToTick);
		W->WriteValue(TEXT("wasEverRegistered"), R.bWasEverRegistered);
		W->WriteValue(TEXT("wasEverEnabled"), R.bWasEverEnabled);
		W->WriteValue(TEXT("observedSeconds"), R.ObservedSeconds);
		W->WriteValue(TEXT("fault"),
			UTickLedgerStatics::FaultName(UTickLedgerStatics::ClassifyRecord(R, Budget)));
		W->WriteValue(TEXT("note"), UTickLedgerStatics::FaultText(R, Budget));
		W->WriteObjectEnd();
	}
	W->WriteArrayEnd();
	W->WriteObjectEnd();
	W->Close();

	const FString Ordner = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("TickLedger"));
	const FString Voll = Ordner / (FileName + TEXT(".json"));
	if (!FFileHelper::SaveStringToFile(Aus, *Voll))
	{
		UE_LOG(LogTickLedger, Warning, TEXT("TickLedger: could not write %s"), *Voll);
		return FString();
	}
	UE_LOG(LogTickLedger, Display, TEXT("TickLedger: report -> %s"), *Voll);
	return Voll;
}

void UTickLedgerSubsystem::StartGate(float Seconds, bool bExitWhenDone)
{
	bGateRunning = true;
	bGateExits = bExitWhenDone;
	GateSeconds = FMath::Max(0.1f, Seconds);
	GateElapsed = 0.f;
	UE_LOG(LogTickLedger, Display, TEXT("TickLedger.Gate: watching for %.1f s%s"),
		GateSeconds, bGateExits ? TEXT("") : TEXT(" (will not exit)"));
}

void UTickLedgerSubsystem::FinishGate()
{
	bGateRunning = false;

	// NOCH EINMAL ABTASTEN, BEVOR GEURTEILT WIRD. Sonst entscheidet das Tor auf einem Stand,
	// der bis zu einem Takt alt ist — bei 4 Hz eine Viertelsekunde, in der ein Objekt seine
	// Tick-Funktion noch angemeldet haben kann.
	SampleWorld();

	const FString Pfad = WriteReport();
	const ETickVerdict Urteil = GetVerdict();
	const int32 Code = UTickLedgerStatics::VerdictExitCode(Urteil);

	UE_LOG(LogTickLedger, Display, TEXT("%s"), *SummaryLine());
	const FString Auffaellig = DominantLine();
	if (!Auffaellig.IsEmpty())
	{
		UE_LOG(LogTickLedger, Display, TEXT("%s"), *Auffaellig);
	}
	UE_LOG(LogTickLedger, Display, TEXT("TickLedger.Gate: %s -> exit code %d (report: %s)"),
		*UTickLedgerStatics::VerdictText(Urteil), Code, *Pfad);

	if (!bGateExits)
	{
		UE_LOG(LogTickLedger, Display,
			TEXT("TickLedger.Gate: -noexit given, staying alive. The exit code WOULD be %d."),
			Code);
		return;
	}

	// GENAU EIN AUSSTIEGSWEG, und Force=true.
	//
	// Bei JointLedger stand hier zusaetzlich ein aufgeschobenes `quit_editor`. Das laeuft im
	// naechsten Tick und beendet den Editor auf dem NORMALEN Weg — also mit Rueckgabe 0. Zwei
	// Ausstiege sind ein Wettrennen, und die 0 gewinnt manchmal. Ohne `Force` bekaeme die
	// Shell ohnehin immer 0; elf Plugins hatten diesen Fehler einmal.
	FPlatformMisc::RequestExitWithStatus(/*Force=*/true, static_cast<uint8>(Code));
}
