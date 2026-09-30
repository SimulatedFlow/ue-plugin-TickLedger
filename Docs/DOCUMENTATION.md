# TickLedger — Documentation

**Unreal Engine 5.8 · Win64 · full C++ source · one runtime module · no third-party code**

## What it does

TickLedger watches the running world and records, for every actor and every component, three
facts: whether it **wants** to tick (`bCanEverTick`), whether its tick function was **ever
registered**, and whether it was **ever enabled**. From those three it separates a wiring
mistake from a deliberate choice.

It is a measurement tool. It registers nothing, enables nothing and disables nothing.

## Install

Copy the plugin into `YourProject/Plugins/TickLedger` and enable it. It is a runtime module and
needs no editor module; nothing has to be placed in your level. The subsystem starts with any
game or PIE world.

## The fault it was built for

`AActor::RegisterActorTickFunctions` (`Actor.cpp`) reads the flag exactly once:

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

There is no `else` branch, and there is nothing to log: a false flag here is the normal case for
most objects. If the flag is turned on **after** this ran — in `BeginPlay`, in an init call, in
a construction script, behind a condition that was not met yet, from a data table — the object
carries `bCanEverTick = true` and has no registered tick function. `Tick()` never runs.

`UActorComponent::SetupActorComponentTickFunction` (`ActorComponent.cpp`) has the same shape and
returns `false` for the same reason. Components are the quieter half of this fault.

## The four verdicts

| Verdict | Meaning | Severity |
|---|---|---|
| `NEVER REGISTERED` | wants to tick, was never registered — `Tick()` never ran | **error** |
| `never enabled` | registered and ready, never switched on in this run | warning |
| `does not tick` | `bCanEverTick` is false | **not a finding** |
| `not judged` | watched for less than `MinObservedSeconds` | abstention |

They are decided in that order, and the order carries two decisions:

* **Abstention comes first.** Without a denominator there is no "never".
* **"Does not tick" is an exclusion, not a finding** — checked before either fault. Otherwise
  every silent object in the scene would fall through into "never registered", and silent is the
  majority.
* **The cause is reported instead of the effect.** Something that was never registered was never
  enabled either; reporting both would be two lines for one mistake. The check is written so
  this holds by substance, not merely by the order of the `if`s.

## Settings

`Project Settings → Plugins → TickLedger`, or `DefaultGame.ini`.

| Setting | Default | What it does |
|---|---|---|
| `SampleHz` | `4.0` | how often the world is sampled |
| `MinObservedSeconds` | `5.0` | below this an object is not judged |
| `MaxWantsTickButNeverRegistered` | `0` | budget for the error — zero, because one is one too many |
| `MaxRegisteredButNeverEnabled` | `8` | budget for the warning |
| `bDoesNotWantToTickIsAFinding` | `false` | list silent objects too. **Recommended off** |
| `bReportRegisteredButNeverEnabled` | `true` | report the warning at all |
| `bWantsTickButNeverRegisteredIsError` | `true` | off turns the error into a warning, never into silence |
| `bNoTickCandidatesIsAnError` | `true` | a run with no candidate checked nothing |
| `bReportUnjudged` | `true` | show abstentions instead of hiding them |
| `bIncludeComponents` | `true` | walk components as well as actors |
| `MaxTrackedObjects` | `512` | cap; when it is hit the report says so |

**`bDoesNotWantToTickIsAFinding` is off on purpose**, and it is the setting that decides whether
this tool is usable. Most objects in a scene do not tick, and that is the recommended default.
With this on, a normal level produces hundreds of entries and a healthy share that always looks
bad — correctly computed and useless. When you do switch it on, every silent object is listed
with no budget at all, because a threshold would suppress exactly the list you asked for.

**A switched-off error becomes a warning, never silence.** Somebody who turns
`bWantsTickButNeverRegisteredIsError` off wants a green gate, not a disappeared finding.

## Console commands

| Command | What it does |
|---|---|
| `TickLedger.Show` / `.Hide` | draw or hide the panel |
| `TickLedger.Reset` | forget everything and start the run over |
| `TickLedger.Dump` | print every object **with a finding** to the log |
| `TickLedger.Classes` | group the findings by class |
| `TickLedger.Report [path]` | write the JSON report |
| `TickLedger.Gate <seconds> [-noexit]` | measure, report, exit 0/1/2 |

**`.Classes` is the command that helps while hunting.** On this tool the individual name is
rarely the answer: if thirty entries are called `BP_Turret_C_0` through `_29`, that is **one**
cause and not thirty. The class shows it, the list of names hides it.

`.Dump` prints only objects with a finding — and when there are none it says so in words,
because an empty list is not good news until the reader knows why it is empty.

## The report

`Saved/TickLedger/report.json`. Besides the per-object records it carries `wantedHz` beside
`achievedHz`, `longestGapSeconds`, `anyCandidateSeen`, `hitTrackingCap` and the full `limitsText`
— the same eight limits the panel prints.

This is a real one, from the demo map that ships with the plugin — not an illustration:

```json
{
  "tool": "TickLedger",
  "verdict": "FAIL",
  "exitCode": 2,
  "summary": "TickLedger FAIL | 70 object(s) | 78 sample(s) @ 3.9 Hz (4.0 wanted) in 20.0 s | healthy 94% | 2 never registered, 2 never enabled, 0 silent, 0 not judged",
  "objectsSeen": 70,
  "samplesTaken": 78,
  "wantedHz": 4.0,
  "achievedHz": 3.897,
  "longestGapSeconds": 0.484,
  "anyCandidateSeen": true,
  "hitTrackingCap": false,
  "healthySharePercent": 94.28,
  "neverRegistered": 2,
  "neverEnabled": 2,
  "doesNotWantToTick": 0,
  "notJudged": 0
}
```

Two numbers in there are worth a second look. `doesNotWantToTick` is **0** although most of
those 70 objects do not tick — with the recommended default they are not findings, so they are
not counted. And `healthySharePercent` stays at **94 %** for the same reason: a report that
opened on a bad number every time would not be read twice.

The second `never enabled` in that run is not from the demo stage at all. It is
`GameplayDebuggerCategoryReplicator`, an engine actor that registers its tick function and only
enables it when the debugger is running — exactly the case this tool calls a warning and not an
error.

`healthySharePercent` is `null`, never `0`, when nothing was judgeable. Zero is a number
somebody measured; `null` is the honest answer when there is no denominator.

## The gate

```
UnrealEditor-Cmd.exe YourProject.uproject /Game/Maps/YourMap -game -unattended \
  -ExecCmds="TickLedger.Gate 60"
```

* **0** — clean
* **1** — warnings (never-enabled over budget, or silent objects listed when you asked for them)
* **2** — errors (never-registered over budget, or no tick candidate seen at all, or no world)

`-noexit` runs the measurement and logs what the exit code *would* be without ending the
process.

The gate samples once more before it judges: at 4 Hz the last reading is otherwise up to a
quarter second old, and an object can register in that time. It exits through exactly one path,
with `Force=true` — two exit paths are a race, and the 0 sometimes wins.

If you give the gate less time than `MinObservedSeconds`, it warns you: every object would be
"not judged" and the run would be green without having checked anything.

## Limits

1. It records what each object was in **this run**. An actor that never spawned is not in the
   report at all — this tool watches what ran, it does not search your project.
2. An object watched for less than `MinObservedSeconds` is not judged.
3. `bCanEverTick = false` is **not a finding**.
4. "Never enabled" is a warning, never an error.
5. It keeps what each object **ever** was, not the state at the end.
6. It tracks at most `MaxTrackedObjects` objects; when the cap is hit the report says so.
7. It is **not a profiler**: registration and enable state, never milliseconds and never tick
   cost.
8. It changes nothing — no tick function is registered, enabled or disabled.

## Tests

Eight automation tests under `TickLedger.*`, run with
`-ExecCmds="Automation RunTests TickLedger"`.

Seven cover the rules above; the eighth is a **feasibility probe** that builds two actors in a
real world, registers one and not the other, and counts their ticks — the measurement the whole
plugin rests on, checked against reality rather than assumed.

Each rule was also **sabotaged once** and the tests were re-run: all ten sabotages were caught.
A suite that has never been attacked only proves that it runs.

## Technical

One runtime module, no editor module. A `UTickableWorldSubsystem` samples, an `AHUD` subclass
draws the panel on `UCanvas`, settings come from `UDeveloperSettings`, and all logic lives in a
pure `UTickLedgerStatics` Blueprint library so it can be tested without a world. No widget, no
material, no asset — it survives a cooked shipping build.

The record key is `(owner, object)`. Keying by object name alone would merge `SceneComponent_0`
across thirty actors into one entry, and a finding would vanish behind a healthy neighbour.

Dependencies: Core, CoreUObject, Engine, DeveloperSettings; privately Json, JsonUtilities and
RenderCore. `FTickFunction`, `AActor::PrimaryActorTick` and `UActorComponent` all live in Engine.
