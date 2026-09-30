// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "TickLedger.h"

#include "TickLedgerLog.h"

#define LOCTEXT_NAMESPACE "FTickLedgerModule"

DEFINE_LOG_CATEGORY(LogTickLedger);

void FTickLedgerModule::StartupModule()
{
}

void FTickLedgerModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTickLedgerModule, TickLedger)
