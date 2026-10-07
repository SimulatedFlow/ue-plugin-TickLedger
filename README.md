# TickLedger — The Tick That Never Ran

**Unreal Engine 5.8 · Win64 · full C++ source · no third-party code · one runtime module**

`bCanEverTick = true` is a wish, not a guarantee. Registration reads that flag **once**, and if
the flag is turned on afterwards nothing happens: no tick function is registered, `Tick()` never
runs, there is no error and nothing appears in the log. The actor carries a flag that says it
ticks, and it does not. You find out when something stops moving and every line of the code
looks correct.

TickLedger walks every actor and component in the running world and records, for each one,
whether it was **ever** registered and **ever** enabled.

```
TickLedger FAIL | 70 object(s) | 78 sample(s) @ 3.9 Hz (4.0 wanted) in 20.0 s
           | healthy 94% | 2 never registered, 2 never enabled, 0 silent, 0 not judged

object                  owner                     wants regd   enabl  note
Probe_NeverRegistered   -                         yes   NO     no     NEVER REGISTERED -
  bCanEverTick is true, but the tick function was never registered, so Tick() never ran.
  There is no error and nothing in the log. This happens when the flag is switched on AFTER
  registration already went past
Comp_NeverRegistered    TickLedgerDemoDirector_0  yes   NO     no     NEVER REGISTERED -
  … - on a component, which is the easier half to overlook
Probe_NeverEnabled      -                         yes   yes    no     never enabled -
  registered and ready, but never switched on in this run. A warning, not an error: an actor
  that waits for its cue looks exactly like one whose cue never came
```

Every number above comes from a real run of the demo map that ships with the plugin.

## Why the engine says nothing

`AActor::RegisterActorTickFunctions`, in `Actor.cpp`:

```cpp
if(bRegister)
{
    if(PrimaryActorTick.bCanEverTick)
    {
        PrimaryActorTick.Target = this;
        PrimaryActorTick.SetTickFunctionEnable(PrimaryActorTick.bStartWithTickEnabled || PrimaryActorTick.IsTickFunctionEnabled());
        PrimaryActorTick.RegisterTickFunction(GetLevel());
    }
}
```

There is **no `else`**. A false flag at this moment is not a mistake to report — it is the
normal case for most objects in a scene. So the branch is simply skipped, and nothing records
that it was. Turning the flag on later changes what the flag *says* and nothing else.

`UActorComponent::SetupActorComponentTickFunction` has the same shape one level down, and
returns `false` for the same reason. **Components are the easier half to overlook**, and a tool
that only walks actors misses the more common case.

## "Does not tick" is NOT a finding

This is the single decision that separates this tool from a naive one. Most objects in a scene
have `bCanEverTick = false` — a floor, a wall, a light, a static mesh — and that is the
**recommended default**. A tool that reported it would flag hundreds of objects per level, the
report would open on a bad number every time, and nobody would read the third one.

So "wants to tick" is an **exclusion that comes before every finding**, not a finding itself.
Silent objects do not drag the healthy share down either: in the run above, 94 % healthy counts
them as healthy, because they are.

## "Ever", not "at the end"

The ledger keeps what each object **ever** was, not its state when the run stopped. Something
that ticked for a minute and was switched off afterwards did its job; reading only the final
state would report it as broken. The two cases look identical at the end and are opposites.

## What a finding does *not* mean

* **It is not a claim about your project.** TickLedger watches what *ran*. An actor that never
  spawned is not in the report at all — this is not a project scanner.
* **`bCanEverTick = false` is not a finding.** See above. That distinction is the whole point.
* **"Never enabled" is a warning, never an error.** An actor waiting for its cue is
  indistinguishable from one whose cue never came. It is reported because that is also where a
  forgotten `SetActorTickEnabled(true)` hides.
* **It is not a profiler.** Registration and enable state, never milliseconds and never tick
  cost. Nothing here is a performance statement.
* **It changes nothing.** No tick function is registered, enabled or disabled by this plugin.
* **The list can be capped.** It tracks at most **512** objects; when the cap is hit the report
  says so and the counts are incomplete. A silent cap would read like completeness.

## It can say "I don't know"

Not judged is not a finding and never changes the verdict. An object is not judged when it was
watched for less than `MinObservedSeconds` — **5.0 s** by default.

Without that floor a short run reports every late spawn as broken: correctly computed and
completely worthless. The gate warns you when you give it less time than the floor, because a
green run that judged nothing is the most expensive kind of green.

A run in which **no object wanted to tick at all** is an **error**, not a pass. It checked
nothing.

## Honest numbers

The report prints the **achieved** sample rate beside the wanted one, plus the longest gap
between two samples — 0.48 s in the run above. Where there is no denominator, there is no
number: with nothing judgeable the healthy share reads `n/a` and the JSON carries `null`, never
`0`, which would be a figure nobody measured.

## A gate for your build server

```
UnrealEditor-Cmd.exe YourProject.uproject /Game/Maps/YourMap -game -unattended \
  -ExecCmds="TickLedger.Gate 60"
```

Exit code **0** clean, **1** warnings, **2** errors. All three are measured from the shell, not
read out of the log. The gate samples once more before it judges — at 4 Hz the last reading is
otherwise up to a quarter second old, and an object can register in that time. The report lands
in `Saved/TickLedger/report.json`.

Console commands: `TickLedger.Show`, `.Hide`, `.Reset`, `.Dump`, `.Classes`, `.Report`,
`.Gate`. **`.Classes` is the one that helps while hunting**: thirty entries named
`BP_Turret_C_0` through `_29` are **one** cause, not thirty, and a list of names hides that
while a list of classes shows it.

## The demo map shows it rather than claiming it

`L_TickLedgerDemo` builds four stations, left to right: healthy, **never registered** (error),
never enabled (warning), silent (no finding) — plus a component with the same fault as the
second, so the quieter half is in the report too.

**Three of the four cubes stand still, and the ledger gives them three different verdicts.**
The eye sees the same thing three times. If the three were distinguishable by looking, nobody
would need this tool.

The fault is produced the way it happens in a real project — the flag is off while registration
goes past and is switched on afterwards — not faked by a switch inside the plugin. The healthy
cube rises and turns **out of `Tick` and out of nothing else**: if the report says a tick ran
and the cube stands still, the report is wrong, and you can see it without reading a number.

## Technical

One runtime module, no editor module. A tickable world subsystem samples the world, an `AHUD`
subclass draws the box with `UCanvas` — no widget, no material, no asset. It survives a cooked
shipping build.

Dependencies: Core, CoreUObject, Engine and DeveloperSettings. `FTickFunction`,
`AActor::PrimaryActorTick` and `UActorComponent` all live in **Engine**; there is nothing else
to add, and an unnecessary dependency is as much a guess as a missing one.

<!-- SF-STORE-BLOCK:BEGIN -->
## 🛒 Source-available — see before you buy

This repository contains the **full source** of a commercial Unreal Engine plugin. It is **source-available, not open source**: read it, evaluate it, then buy a license to use it. See **the Fab Content License Agreement / Unreal Engine EULA (purchase required)**.

**Get it / Buy:**
- **Buy on Fab** (this plugin): https://www.fab.com/listings/bc128edd-a271-4265-ae2f-8fe85053885c
- Fab store — all our UE5 plugins: https://www.fab.com/sellers/Silvan%20Teufel

### 📬 **Free UE5 Snippet-Pack**

10 ready-to-use C++/Blueprint building blocks (subsystems, versioned saves, async nodes, editor tooling) — MIT licensed. Get it by joining the newsletter — plus a heads-up when something new ships. Double opt-in, unsubscribe in one click, no address sharing.

👉 **[Get the free pack](https://silvan.teufel-engineering.com/newsletter/plugins/?q=gh)**

_© 2026 Silvan Teufel. All rights reserved._
<!-- SF-STORE-BLOCK:END -->
