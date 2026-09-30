// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TickLedgerDemoProbe.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Eine Station der Demo: ein Wuerfel, der sich bewegt SOLANGE er tickt, und eine Saeule, die
 * das Urteil des Plugins traegt.
 *
 * WARUM DAS NICHT `ATickLedgerTickProbe` IST. Die Sonde im Produkt zaehlt und tut sonst nichts
 * — kein Mesh, kein Material, keine Logik —, und das steht so in ihrem Kopf. Haenge ich ihr
 * fuer die Demo zwei Meshes an, ist die Begruendung hinfaellig und der Kaeufer bekommt eine
 * Gegenprobe, die selbst etwas darstellt. Die Trennung kostet eine Datei.
 *
 * DIE BEWEGUNG IST DER EIGENTLICHE BEWEIS. Das Plugin behauptet „Tick lief nie". Ein Wuerfel,
 * der stillsteht, belegt das UNABHAENGIG vom Plugin — er wird aus `Tick` heraus bewegt und aus
 * sonst nichts. Stimmen Bild und Bericht nicht ueberein, ist einer von beiden falsch, und man
 * sieht es sofort.
 */
UCLASS()
class TICKLEDGER_API ATickLedgerDemoProbe : public AActor
{
	GENERATED_BODY()

public:
	ATickLedgerDemoProbe();

	virtual void Tick(float DeltaSeconds) override;

	/** Mesh und Material nachtraeglich setzen — die Assets laedt der Regisseur. */
	void Configure(UStaticMesh* Mesh, UMaterialInterface* Material);

	/** Die Saeule einfaerben. Die Farbe kommt vom Plugin, nicht von der Erwartung. */
	void SetVerdictColour(const FLinearColor& Colour);

	/** Wie oft `Tick` WIRKLICH gelaufen ist. Das ist die Zahl, die nichts beschoenigt. */
	UPROPERTY(BlueprintReadOnly, Category = "TickLedger|Demo")
	int32 TickCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TickLedger|Demo")
	float AliveSeconds = 0.f;

	/** Ruhehoehe des Wuerfels. Wer nicht tickt, bleibt hier stehen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger|Demo")
	float BaseHeight = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger|Demo")
	float RiseHeight = 220.f;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Cube;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Pillar;

	/**
	 * EINMAL ANLEGEN, NICHT JE BILD.
	 *
	 * Der Regisseur faerbt in jedem Tick nach — das Urteil kann sich waehrend des Laufs aendern,
	 * und die Saeule soll das zeigen. `CreateAndSetMaterialInstanceDynamic` legt dabei aber JEDES
	 * MAL ein neues Exemplar an: bei vier Stationen und 80 Bildern in der Sekunde sind das ueber
	 * 300 Objekte je Sekunde, die nur auf den Sammler warten. In einer 20-Sekunden-Demo faellt
	 * das nicht auf; in einer Szene, aus der jemand abschreibt, schon.
	 */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PillarMID;
};
