# TickLedger - The Tick That Never Ran

`bCanEverTick = true` is a wish, not a guarantee.

Registration reads that flag exactly once. If the flag is turned on afterwards - in `BeginPlay`,
in an init call, in a construction script, behind a condition that was not met yet - nothing
happens. No tick function is registered, `Tick()` never runs, there is no error, and nothing
appears in the log. The actor carries a flag that says it ticks, and it does not.

You find out when something stops moving and every line of the code looks correct.

TickLedger walks every actor and component in the running world and records, for each one,
whether it was **ever** registered and **ever** enabled.

```
TickLedger FAIL | 70 object(s) | 78 sample(s) @ 3.9 Hz (4.0 wanted) in 20.0 s
           | healthy 94% | 2 never registered, 2 never enabled, 0 silent, 0 not judged

object                  owner                     wants regd   enabl  note
Probe_NeverRegistered   -                         yes   NO     no     NEVER REGISTERED -
  bCanEverTick is true, but the tick function was never registered, so Tick() never ran.
  There is no error and nothing in the log.
Comp_NeverRegistered    TickLedgerDemoDirector_0  yes   NO     no     NEVER REGISTERED -
  ... - on a component, which is the easier half to overlook
Probe_NeverEnabled      -                         yes   yes    no     never enabled -
  registered and ready, but never switched on in this run. A warning, not an error.
```

Every number above comes from a real run of the demo map that ships with the plugin.

## Why the engine stays quiet

`AActor::RegisterActorTickFunctions` has no `else` branch for a false flag - and it should not
have one. Most objects in a scene do not tick, and that is the recommended default. So the
branch is skipped, nothing records that it was skipped, and turning the flag on later changes
what the flag *says* and nothing else.

`UActorComponent::SetupActorComponentTickFunction` has the same shape one level down. Components
are the quieter half of this fault, and a tool that only walks actors misses the more common
case.

## "Does not tick" is NOT a finding

This is the decision that makes the tool usable. Most objects in a scene have
`bCanEverTick = false`. A tool that reported them would flag hundreds of objects per level, open
on a bad number every time, and nobody would read the third report.

So "wants to tick" is an exclusion that comes **before** every finding, not a finding itself.
Silent objects do not drag the healthy share down either - in the run above, 94 % healthy counts
them as healthy, because they are.

## "Ever", not "at the end"

The ledger keeps what each object **ever** was, not its state when the run stopped. Something
that ticked for a minute and was switched off afterwards did its job. Reading only the final
state would report it as broken - and the two cases look identical at the end.

## What a finding does *not* mean

* **It is not a claim about your project.** It watches what *ran*. An actor that never spawned
  is not in the report at all; this is not a project scanner.
* **`bCanEverTick = false` is not a finding.** That distinction is the whole point.
* **"Never enabled" is a warning, never an error.** An actor waiting for its cue is
  indistinguishable from one whose cue never came - which is also where a forgotten
  `SetActorTickEnabled(true)` hides.
* **It is not a profiler.** Registration and enable state, never milliseconds and never tick
  cost.
* **It changes nothing.** No tick function is registered, enabled or disabled.
* **The list can be capped.** At most 512 objects; when the cap is hit the report says so. A
  silent cap would read like completeness.

## It can say "I don't know"

An object watched for less than 5 s is **not judged**, and an abstention never changes the
verdict. Without that floor a short run reports every late spawn as broken: correctly computed
and completely worthless.

A run in which no object wanted to tick at all is an **error**, not a pass. It checked nothing.

Where there is no denominator, there is no number: the healthy share reads `n/a` and the JSON
carries `null`, never `0`.

## A gate for your build server

```
UnrealEditor-Cmd.exe YourProject.uproject /Game/Maps/YourMap -game -unattended \
  -ExecCmds="TickLedger.Gate 60"
```

Exit code **0** clean, **1** warnings, **2** errors, and the report lands in
`Saved/TickLedger/report.json`.

The world is sampled at **4 Hz** by default, and the report prints the rate it actually
achieved beside the one you asked for, plus the longest gap between two samples. The gate
samples once more before it judges - at 4 Hz the last reading is otherwise up to a quarter
second old, and an object can register in that time. It also warns you when you give it less
time than the observation floor: a green run that judged nothing is the most expensive kind of
green.

Console commands: `TickLedger.Show`, `.Hide`, `.Reset`, `.Dump`, `.Classes`, `.Report`, `.Gate`.

**`.Classes` is the one that helps while hunting**: thirty entries named `BP_Turret_C_0` through
`_29` are one cause, not thirty. The class shows that; a list of names hides it.

## The demo map shows it rather than claiming it

`L_TickLedgerDemo` builds four stations: healthy, never registered (error), never enabled
(warning), silent (no finding) - plus a component carrying the same fault as the second.

**Three of the four cubes stand still, and the ledger gives them three different verdicts.** The
eye sees the same thing three times. If the three were distinguishable by looking, nobody would
need this tool.

The fault is produced the way it happens in a real project, not faked by a switch inside the
plugin. The healthy cube rises and turns out of `Tick` and out of nothing else - so if the
report claims a tick ran and the cube stands still, you can see the contradiction without
reading a number.

## What you get

* Full C++ source, one runtime module, no editor module, no third-party code.
* A tickable world subsystem, an `AHUD` panel drawn on `UCanvas`, `UDeveloperSettings`, and all
  logic in a pure Blueprint function library - no widget, no material, no asset. It survives a
  cooked shipping build.
* 18 Blueprint-callable functions, seven console commands, a JSON report and a build-server gate.
* Eight automation tests, including a feasibility probe that registers one actor and not
  another, in a real world, and counts their ticks. Every rule was sabotaged once and the tests
  were re-run: all ten sabotages were caught.
* The demo map, the demo director and the tick probe, with source.

Unreal Engine 5.8 · Win64.
