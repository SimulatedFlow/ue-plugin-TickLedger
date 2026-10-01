// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "TickLedgerHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "TickLedgerLog.h"
#include "TickLedgerSettings.h"
#include "TickLedgerStatics.h"
#include "TickLedgerSubsystem.h"

ATickLedgerHUD::ATickLedgerHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATickLedgerHUD::ToggleOverlay()
{
	// Die Sichtbarkeit haengt an der Einstellung, nicht an einem zweiten Schalter im HUD —
	// sonst gibt es zwei Wahrheiten darueber, ob der Kasten zu sehen sein soll.
	UTickLedgerSettings* S = GetMutableDefault<UTickLedgerSettings>();
	S->bShowOverlay = !S->bShowOverlay;
}

void ATickLedgerHUD::DrawHUD()
{
	Super::DrawHUD();

	if (Canvas && UTickLedgerSettings::Get()->bShowOverlay)
	{
		DrawPanel(Canvas, PanelOrigin, PanelWidth);
	}
}

void ATickLedgerHUD::DrawPanel(UCanvas* InCanvas, const FVector2D& Origin, float Width)
{
	if (!InCanvas)
	{
		return;
	}
	UTickLedgerSubsystem* S = UTickLedgerSubsystem::Get(this);
	if (!S)
	{
		return;
	}
	UFont* Schrift = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Schrift)
	{
		return;
	}

	const FTickBudget Budget = S->GetBudget();

	// SCHON SORTIERT — `GetFindingsBySeverity` tut das, und zwar fuer Kasten, Bericht und
	// Kopfzeile gemeinsam. Eine eigene Rangfolge im Kasten waere die erste Stelle, an der
	// Anzeige und Urteil auseinanderlaufen.
	TArray<FTickRecord> MitUrteil = S->GetFindingsBySeverity();
	MitUrteil.RemoveAll([&Budget](const FTickRecord& R)
	{
		// „WILL NICHT TICKEN" GEHOERT HIER GENAUSO WENIG HIN WIE DIE ENTHALTUNG.
		//
		// Aus der JointLedger-Fassung uebernommen war nur die Enthaltung gefiltert. Bei
		// diesem Werkzeug waere das ein handfester Fehler: schaltet jemand
		// `bDoesNotWantToTickIsAFinding` ein, stehen in einer gewoehnlichen Szene HUNDERTE
		// stille Objekte in der Liste — und die Zeile am Ende meldete „… and 500 more
		// object(s) with a finding", obwohl kein einziges davon ein Befund ist.
		if (UTickLedgerStatics::ClassifyRecord(R, Budget) == ETickFault::DoesNotWantToTick)
		{
			return true;
		}
		// Enthaltungen stehen im Bericht, aber nicht in der Fundliste des Kastens: sie sind
		// kein Befund, und zwischen echten Befunden gelesen sehen sie wie welche aus.
		return UTickLedgerStatics::ClassifyRecord(R, Budget) == ETickFault::Unjudged;
	});

	// Die Grenzen sind lang und mehrzeilig. Sie werden UMGEBROCHEN und nicht abgeschnitten:
	// Abschneiden waere die bequemste Art, eine Einschraenkung verschwinden zu lassen.
	TArray<FString> GrenzTeile;
	S->LimitsLine().ParseIntoArrayLines(GrenzTeile, false);

	const int32 ZeilenGezeigt = FMath::Min(MitUrteil.Num(), FMath::Max(1, MaxRecordRows));
	const int32 ZeilenVersteckt = FMath::Max(0, MitUrteil.Num() - ZeilenGezeigt);

	const float Zeile = 15.f;
	const float Kopf = Zeile * (2.f + GrenzTeile.Num() + (Note.IsEmpty() ? 0.f : 1.f));
	const float ZeilenBlock = Zeile * (1.f + FMath::Max(1, ZeilenGezeigt)
		+ (ZeilenVersteckt > 0 ? 1.f : 0.f));
	const float Hoehe = 8.f + Kopf + ZeilenBlock + 10.f;

	FCanvasTileItem Kasten(FVector2D(Origin.X, Origin.Y), FVector2D(Width, Hoehe),
		FLinearColor(0.02f, 0.04f, 0.09f, 0.82f));
	Kasten.BlendMode = SE_BLEND_Translucent;
	InCanvas->DrawItem(Kasten);

	float Y = Origin.Y + 6.f;
	const float X = Origin.X + 10.f;
	auto Schreib = [&](const FString& Text, const FLinearColor& Farbe)
	{
		FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Schrift, Farbe);
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f));
		InCanvas->DrawItem(Item);
		Y += Zeile;
	};

	const FLinearColor Matt(0.62f, 0.70f, 0.78f);
	const FLinearColor Weiss(0.86f, 0.92f, 0.98f);
	const FLinearColor Gruen(0.38f, 0.92f, 0.52f);

	Schreib(S->SummaryLine(), UTickLedgerStatics::VerdictColour(S->GetVerdict()));

	// Der schwerste Befund im Klartext. Er ist der Grund, das Plugin zu behalten, und gehoert
	// nicht in einer Tabellenspalte vergraben.
	{
		const FString Dom = S->DominantLine();
		if (!Dom.IsEmpty())
		{
			TArray<FString> DomTeile;
			Dom.ParseIntoArrayLines(DomTeile, false);
			for (const FString& T : DomTeile)
			{
				Schreib(T, Weiss);
			}
		}
		else if (S->HasSeenAnyCandidate())
		{
			Schreib(TEXT("every object that wanted to tick was registered for it"), Gruen);
		}
		else
		{
			// KEIN TICK-KANDIDAT HEISST NICHT „ALLES GUT". Das ist der gefaehrlichste gruene
			// Nullwert, den ein pruefendes Werkzeug liefern kann.
			Schreib(TEXT("nothing in this run wanted to tick - nothing was checked, and that is not "
				"a pass"), UTickLedgerStatics::VerdictColour(ETickVerdict::Fail));
		}
	}

	for (const FString& T : GrenzTeile)
	{
		Schreib(T, Matt);
	}

	if (!Note.IsEmpty())
	{
		Schreib(Note, FLinearColor(1.00f, 0.84f, 0.35f));
	}

	Schreib(UTickLedgerStatics::RecordHeader(), Matt);

	if (MitUrteil.Num() == 0)
	{
		Schreib(TEXT("   (no findings)"), Gruen);
	}
	else
	{
		for (int32 i = 0; i < ZeilenGezeigt; ++i)
		{
			Schreib(UTickLedgerStatics::RecordLine(MitUrteil[i], Budget),
				UTickLedgerStatics::FaultColour(
					UTickLedgerStatics::ClassifyRecord(MitUrteil[i], Budget)));
		}
		if (ZeilenVersteckt > 0)
		{
			Schreib(FString::Printf(
				TEXT("   ... and %d more object(s) with a finding - the full list is in the report"),
				ZeilenVersteckt), Matt);
		}
	}
}

FString ATickLedgerHUD::SavePanelImage(int32 Width, int32 Height, const FString& FileName)
{
	UWorld* World = GetWorld();
	if (!World || Width < 16 || Height < 16)
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
	RT->ClearColor = FLinearColor(0.02f, 0.04f, 0.09f, 1.f);
	RT->bAutoGenerateMips = false;
	RT->InitAutoFormat(Width, Height);
	RT->UpdateResourceImmediate(true);
	UKismetRenderingLibrary::ClearRenderTarget2D(World, RT, FLinearColor(0.02f, 0.04f, 0.09f, 1.f));

	UCanvas* C = nullptr;
	FVector2D Groesse = FVector2D::ZeroVector;
	FDrawToRenderTargetContext Kontext;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, RT, C, Groesse, Kontext);
	if (C)
	{
		DrawPanel(C, FVector2D(24.f, 24.f),
			FMath::Min(static_cast<float>(Width) - 48.f, PanelWidth));
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Kontext);

	const FString Ordner = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("TickLedger") / TEXT("Shots"));
	const FString Datei = FileName.IsEmpty() ? TEXT("Panel") : FileName;
	const FString Voll = Ordner / (Datei + TEXT(".png"));

	// NICHT `UKismetRenderingLibrary::ExportRenderTarget`.
	//
	// Der Weg dahinter (`FImageUtils::ExportRenderTarget2DAsPNG`) schreibt
	// `CompressedData.GetAllocatedSize()` statt `.Num()`, also die KAPAZITAET des Puffers.
	// Hinter dem PNG stehen dadurch die ungenutzten Bytes der Allokation. Bildbetrachter
	// ueberlesen das, ffmpeg nicht.
	FImage Bild;
	TArray64<uint8> PNG;
	if (!FImageUtils::GetRenderTargetImage(RT, Bild)
		|| !FImageUtils::CompressImage(PNG, TEXT("PNG"), Bild)
		|| !FFileHelper::SaveArrayToFile(TArrayView64<const uint8>(PNG.GetData(), PNG.Num()), *Voll))
	{
		UE_LOG(LogTickLedger, Warning, TEXT("TickLedger: could not write %s"), *Voll);
		return FString();
	}
	UE_LOG(LogTickLedger, Display, TEXT("TickLedger: panel image -> %s"), *Voll);
	return Voll;
}
