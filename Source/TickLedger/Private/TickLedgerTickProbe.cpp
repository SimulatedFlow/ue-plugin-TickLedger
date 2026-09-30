// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "TickLedgerTickProbe.h"

ATickLedgerTickProbe::ATickLedgerTickProbe()
{
	// ABSICHTLICH TRUE: die Probe soll ticken koennen. Ob sie es DARF, entscheidet die
	// Registrierung — und genau deren Verhaeltnis zu diesem Schalter wird gemessen.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ATickLedgerTickProbe::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	++TickCount;
}
