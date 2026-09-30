// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "TickLedgerDemoProbe.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

ATickLedgerDemoProbe::ATickLedgerDemoProbe()
{
	// GESUND IST DIE VOREINSTELLUNG. Wer diese Klasse ohne Zutun hinstellt, bekommt einen
	// Wuerfel, der tickt. Die drei kaputten Faelle stellt der Regisseur her, indem er VOR dem
	// Fertigstellen an diesen Flaggen dreht — genau so entstehen sie im echten Projekt auch.
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Cube = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cube"));
	Cube->SetupAttachment(GetRootComponent());
	Cube->SetRelativeLocation(FVector(0.f, 0.f, BaseHeight));
	Cube->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Pillar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pillar"));
	Pillar->SetupAttachment(GetRootComponent());
	Pillar->SetRelativeLocation(FVector(0.f, 260.f, 150.f));
	Pillar->SetRelativeScale3D(FVector(0.6f, 0.6f, 3.0f));
	Pillar->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// DIE BEIDEN KOMPONENTEN TICKEN NICHT und sollen es auch nicht. Stuenden sie auf
	// `bCanEverTick = true`, haette jede Station zwei zusaetzliche Eintraege im Bericht, und der
	// Fall „Komponente nie registriert" ginge zwischen acht harmlosen Zeilen unter.
	Cube->PrimaryComponentTick.bCanEverTick = false;
	Pillar->PrimaryComponentTick.bCanEverTick = false;
}

void ATickLedgerDemoProbe::Configure(UStaticMesh* Mesh, UMaterialInterface* Material)
{
	if (!Mesh || !Material)
	{
		return;
	}
	if (Cube)
	{
		Cube->SetStaticMesh(Mesh);
		Cube->SetMaterial(0, Material);
	}
	if (Pillar)
	{
		Pillar->SetStaticMesh(Mesh);
		// ERST DAS MATERIAL TAUSCHEN, DANN SPAETER DAS MID BAUEN. Der Engine-Wuerfel traegt
		// `WorldGridMaterial`, und das hat gar keinen Farbparameter — ein MID darauf nimmt
		// `SetVectorParameterValue` widerspruchslos an und zeigt nichts.
		Pillar->SetMaterial(0, Material);
	}
}

void ATickLedgerDemoProbe::SetVerdictColour(const FLinearColor& Colour)
{
	if (!Pillar)
	{
		return;
	}
	if (!PillarMID)
	{
		PillarMID = Pillar->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (PillarMID)
	{
		PillarMID->SetVectorParameterValue(TEXT("Color"), Colour);
	}
}

void ATickLedgerDemoProbe::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	++TickCount;
	AliveSeconds += DeltaSeconds;

	if (!Cube)
	{
		return;
	}

	// EINE BEWEGUNG, DIE AUF EINEM STANDBILD ABLESBAR IST.
	//
	// Kein Sinus: ein Pendel kommt zur Ruhelage zurueck, und ein Bild, das genau dann entsteht,
	// zeigt einen tickenden Wuerfel an derselben Stelle wie einen toten. Bei SlotLedger standen
	// Aufnahme- und Bewegungstakt beide auf 3 s, und jedes Standbild traf denselben Augenblick —
	// vier Bilder, die aussahen wie eines.
	//
	// Stattdessen steigt der Wuerfel und BLEIBT oben. Ab etwa einer Sekunde ist der Unterschied
	// auf jedem Bild zu sehen, egal wann es entsteht.
	const float Hoehe = RiseHeight * (1.f - FMath::Exp(-AliveSeconds));
	Cube->SetRelativeLocation(FVector(0.f, 0.f, BaseHeight + Hoehe));
	Cube->SetRelativeRotation(FRotator(0.f, AliveSeconds * 80.f, 0.f));
}
