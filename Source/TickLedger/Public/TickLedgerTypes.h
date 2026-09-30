// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TickLedgerTypes.generated.h"

/**
 * Die Befundarten.
 *
 * EIN FEHLER, EINE WARNUNG, EINE ENTHALTUNG — und der wichtigste Fall ist wieder der, der
 * NICHT gemeldet wird: `bCanEverTick = false`. Ein Objekt, das gar nicht ticken will, ist
 * voellig in Ordnung; es ist sogar die empfohlene Voreinstellung, und in einer vollen Szene
 * trifft das auf die grosse Mehrheit zu. Ein Werkzeug, das jedes stille Objekt meldet, ist
 * unbrauchbar — dieselbe Falle wie der Welt-Anker bei JointLedger und `hand_r` bei
 * SocketLedger.
 */
UENUM(BlueprintType)
enum class ETickFault : uint8
{
	/** Kein Befund: das Objekt wollte ticken und war registriert. */
	None UMETA(DisplayName = "None"),

	/**
	 * DER EIGENTLICHE FUND: `bCanEverTick` steht auf true, die Tick-Funktion war aber NIE
	 * registriert.
	 *
	 * Dann laeuft `Tick()` nie, es gibt keinen Fehler und keinen Logeintrag. AM 29.09.2026 AN
	 * ZWEI ECHTEN ACTORS GEMESSEN: beide mit `bCanEverTick = true`, einer registriert, einer
	 * nicht — nach zehn Weltticks zaehlte der registrierte 1 Tick, der andere 0. Der Schluss
	 * „nicht registriert heisst nie gelaufen" ist damit belegt und nicht angenommen.
	 *
	 * So entsteht es in echten Projekten: der Schalter wird angeschaltet, NACHDEM die
	 * Registrierung gelaufen ist — in einer Blueprint-Elternklasse, in einem
	 * Konstruktionsskript, aus einer Datentabelle. FEHLER.
	 */
	WantsTickButNeverRegistered UMETA(DisplayName = "Wants to tick but was never registered"),

	/**
	 * Registriert, aber in dieser Sitzung nie eingeschaltet.
	 *
	 * KEIN FEHLER: genau so baut man ein Objekt, das erst spaeter anlaufen soll — ein Gegner,
	 * der bis zum Ausloeser schlaeft, ein Effekt, der auf sein Stichwort wartet. Gemeldet
	 * wird es trotzdem, weil hier auch das Stichwort liegt, das nie kam. WARNUNG.
	 */
	RegisteredButNeverEnabled UMETA(DisplayName = "Registered but never enabled"),

	/**
	 * Das Objekt will gar nicht ticken — gemeldet NUR auf ausdrueckliches Verlangen.
	 *
	 * DIESE ART GIBT ES, WEIL DAS BUDGET EINEN SCHALTER DAFUER HAT. Bei JointLedger stand ein
	 * solcher Schalter einen ganzen Punkt lang im Budget, ohne dass es etwas zu melden gab —
	 * der Fall fiel durch alle Zweige und landete bei `None`. Ein Schalter, der nichts tut,
	 * ist schlimmer als keiner: sein Besitzer glaubt, er habe etwas eingeschaltet.
	 *
	 * IMMER NUR WARNUNG, nie Fehler. Wer sie einschaltet, will eine Liste — kein rotes Tor.
	 */
	DoesNotWantToTick UMETA(DisplayName = "Does not want to tick"),

	/**
	 * ENTHALTUNG, KEIN FEHLER. Zwei Faelle:
	 *  1. Das Objekt wurde kuerzer beobachtet als `MinObservedSeconds` — „nie" braucht einen
	 *     Nenner, und ein Objekt, das erst spaet gespawnt wurde, hatte keine Gelegenheit.
	 *  2. Es hatte nie einen Namen; dann ist der Eintrag eine Huelse.
	 */
	Unjudged UMETA(DisplayName = "Not judged")
};

/** Das Urteil ueber den ganzen Lauf. Dieselben drei Stufen wie in allen Ledger-Werkzeugen. */
UENUM(BlueprintType)
enum class ETickVerdict : uint8
{
	Pass UMETA(DisplayName = "PASS"),
	Warn UMETA(DisplayName = "WARN"),
	Fail UMETA(DisplayName = "FAIL")
};

/**
 * Die Schwellen. Jede einzelne ist eine Stelle, an der jemand anderer Meinung sein darf —
 * deshalb stehen sie hier und nicht verstreut im Code.
 */
USTRUCT(BlueprintType)
struct TICKLEDGER_API FTickBudget
{
	GENERATED_BODY()

	/**
	 * Wie viele „will ticken, war nie registriert" das Urteil ueberleben.
	 * Voreinstellung 0: dieser Fall ist immer ein Fehler, er sieht nur nicht danach aus.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger", meta = (ClampMin = "0"))
	int32 MaxWantsTickButNeverRegistered = 0;

	/**
	 * Wie viele schlafende Objekte geduldet werden, bevor gewarnt wird. Nicht 0: ein Gegner,
	 * der auf seinen Ausloeser wartet, ist kein Mangel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger", meta = (ClampMin = "0"))
	int32 MaxRegisteredButNeverEnabled = 8;

	/**
	 * WIE LANGE EIN OBJEKT BEOBACHTET SEIN MUSS, BEVOR „NIE" GESAGT WERDEN DARF.
	 *
	 * Der Nenner ist die Sitzung, nicht ein Einzelvorgang — wie bei JointLedger und
	 * LayerLedger, anders als bei SlotLedger. Ein Objekt, das in der letzten Sekunde gespawnt
	 * wurde, hatte keine Gelegenheit zu ticken, und daraus einen Befund zu machen waere eine
	 * Aussage ueber die Messdauer und nicht ueber das Objekt.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger", meta = (ClampMin = "0.0"))
	float MinObservedSeconds = 5.0f;

	/**
	 * IST „WILL GAR NICHT TICKEN" EIN BEFUND?
	 *
	 * VOREINSTELLUNG NEIN, und das ist die wichtigste Entscheidung in dieser Datei. In einer
	 * gewoehnlichen Szene tickt die MEHRHEIT der Objekte nicht — das ist die empfohlene
	 * Bauweise und spart genau die Zeit, um die es geht. Wer das meldet, bekommt eine Liste
	 * mit tausend Eintraegen und darin keinen einzigen Befund.
	 *
	 * Einschaltbar fuer den seltenen Fall, dass jemand wissen will, WAS alles still ist —
	 * dann aber bewusst, und mit `DoesNotWantToTick` als eigener, klar benannter Warnung.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger")
	bool bDoesNotWantToTickIsAFinding = false;

	/** Ist „registriert, nie eingeschaltet" ueberhaupt eine Warnung? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger")
	bool bReportRegisteredButNeverEnabled = true;

	/**
	 * „Will ticken, nie registriert" ist ein Fehler. Abschaltbar, aber nie zu einem stillen
	 * Bestehen: aus dem Fehler wird dann eine WARNUNG.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger")
	bool bWantsTickButNeverRegisteredIsError = true;

	/**
	 * Ein Lauf, in dem ueberhaupt kein tickendes Objekt vorkam, ist KEIN Bestehen — er hat
	 * nichts geprueft. Derselbe gefaehrliche gruene Nullwert wie eine Karte ohne Data Layer
	 * bei LayerLedger.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger")
	bool bNoTickCandidatesIsAnError = true;

	/**
	 * Sollen Enthaltungen im Bericht auftauchen? Voreinstellung ja — eine Enthaltung, die man
	 * nicht sieht, ist von einem uebersehenen Objekt nicht zu unterscheiden.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger")
	bool bReportUnjudged = true;

	/**
	 * Werden auch KOMPONENTEN geprueft?
	 *
	 * Voreinstellung ja, und das ist kein Beiwerk: `UActorComponent::PrimaryComponentTick`
	 * hat dieselbe Struktur und dieselbe Falle wie die des Actors. Ein Werkzeug, das nur
	 * Actors ansieht, uebersieht die Haelfte — und zwar die unauffaelligere.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger")
	bool bIncludeComponents = true;

	/** Obergrenze fuer verfolgte Objekte, damit eine volle Szene das Werkzeug nicht traegt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TickLedger", meta = (ClampMin = "1"))
	int32 MaxTrackedObjects = 512;
};

/**
 * Ein Objekt (Actor oder Komponente) ueber die ganze Sitzung.
 *
 * GESPEICHERT WIRD, WAS JE WAR — nicht der Zustand am Ende. Ein Objekt, das eine Minute lang
 * getickt hat und danach abgeschaltet wurde, hat seine Arbeit getan; wer den Schlusszustand
 * nimmt, meldet es als nie gelaufen. Das ist derselbe Fehler, den SlotLedger mit dem
 * Endgewicht gemacht haette und LayerLedger mit dem letzten Ladezustand.
 */
USTRUCT(BlueprintType)
struct TICKLEDGER_API FTickRecord
{
	GENERATED_BODY()

	/** Der Name des Objekts — die Adresse des Befundes. */
	UPROPERTY(BlueprintReadOnly, Category = "TickLedger")
	FName ObjectName;

	/** Bei einer Komponente: der Actor, an dem sie haengt. Sonst leer. */
	UPROPERTY(BlueprintReadOnly, Category = "TickLedger")
	FName OwnerName;

	/** Die Klasse — ohne sie sagt ein Name wie „SceneComponent_0" nichts. */
	UPROPERTY(BlueprintReadOnly, Category = "TickLedger")
	FName ClassName;

	/** Actor oder Komponente? Die beiden Fallen sind gleich, die Adressen nicht. */
	UPROPERTY(BlueprintReadOnly, Category = "TickLedger")
	bool bIsComponent = false;

	/** Stand `bCanEverTick` jemals auf true? */
	UPROPERTY(BlueprintReadOnly, Category = "TickLedger")
	bool bWantsToTick = false;

	/**
	 * War die Tick-Funktion JEMALS registriert?
	 *
	 * Das hoechste je gesehene Ergebnis, nicht das letzte: ein Objekt, das beim Aufraeumen
	 * abgemeldet wird, war trotzdem registriert.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TickLedger")
	bool bWasEverRegistered = false;

	/** War die Tick-Funktion JEMALS eingeschaltet? Ebenfalls das hoechste Ergebnis. */
	UPROPERTY(BlueprintReadOnly, Category = "TickLedger")
	bool bWasEverEnabled = false;

	/** Wie lange dieses Objekt beobachtet wurde. Der Nenner jeder „nie"-Aussage. */
	UPROPERTY(BlueprintReadOnly, Category = "TickLedger")
	float ObservedSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "TickLedger")
	ETickFault Fault = ETickFault::None;
};
