# Maintenance backlog

Repo-wide problems found while building the `starfield` screensaver (PR #31)
and deliberately scoped out of that PR to keep it focused.

**`starfield` is the reference implementation for every task below — copy it.**
Relevant commits: `3a83df7`, `ff4d076`, `ca982e8`, `03b14a9`, `01a144c`.

The `libs` submodule work in [MAINTENANCE-rslibs.md](MAINTENANCE-rslibs.md) is
**done** — L1 through L5 and the `rsWin32Saver` half of L8 are merged, and the
pointer is at `42d251b` (PR #40). Only L6 and L7 remain there, both breaking API
changes that need coordinated edits here.

All counts below were re-verified against `main` at `bd7b5d8`, and the
SonarCloud figures against the analysis of that commit. Re-check them before
starting; `grep` commands are given with each task.

**Task 17 is finished: all thirteen savers now have tests.** That rollout found
**nine** real defects in total — Tasks 14 to 16 from the first two batches, and
Tasks 18 to 23 from the last one. Seven are fixed. Prefer covering a saver before
changing it; every one of the nine turned up that way, and none of them had
surfaced in thirteen years of the savers running.

One theme runs through six of the nine and is worth reading before touching any
saver: **none of these savers was written to be shut down and started again in
the same process.** Teardown frees memory but leaves the counters, flags and
function-local statics that index it exactly where they were. Harmless in a
screensaver, which exits instead of restarting — and the reason the harness
finds them, because a test fixture restarts constantly.

## Building and verifying

```bash
msbuild src\rssavers.sln /p:Configuration=Release /p:Platform=x86 /t:Rebuild
```

Two traps that cost time:

- Use `/t:Rebuild`, not `Build`. Compiler warnings such as `D9035` only appear
  when the source is actually recompiled, so a plain `Build` will look clean.
- Close any running executable from `bin\` first, or the link fails with
  `LNK1104: cannot open file`.

Build the solution, never a single `.vcxproj`. The solution is what builds the
five lib projects, which it references via `..\libs\...`; alone, a saver project
finds no `rsWin32Saverd.lib` to link.

Unit tests — one binary per saver, plus the settings-only suite:

```bash
cmake -S tests -B tests/build -A Win32
cmake --build tests/build --config Debug
ctest --test-dir tests/build -C Debug --output-on-failure
```

Coverage locally, which is what CI reports on every PR. Debug only: Release
inlining makes line attribution meaningless, and OpenCppCoverage needs the PDBs
this configuration produces.

```bash
OpenCppCoverage.exe --sources %CD%\src --excluded_sources %CD%\tests --cover_children --export_type cobertura:coverage.xml -- ctest --test-dir tests/build -C Debug
```

```bash
pwsh tests/tools/coverage-report.ps1 -ReportPath coverage.xml
```

Do not pass `--timeout` to that `ctest`: under instrumentation the heavier tests
exceed a short per-test limit, and the CI job passes no timeout for that reason.

**That coverage run was 40 minutes and is now under 6**, at unchanged coverage.
Instrumented execution is the whole cost — the same tests run in about 20 seconds
without it.

| | Time | Coverage |
|---|---:|---:|
| original, serial | 40 min | 71.9% |
| after the test-volume cuts below, serial | ~13 min | 72.1% |
| **plus `ctest -j`** | **5.8 min** | **72.1%** |

`-j` is most of the win and cost nothing: ~300 independent processes, so they
parallelise almost linearly, and OpenCppCoverage merges concurrent children
correctly — the totals agree to four significant figures across `-j 4` and
`-j 8`. It is sized from `[Environment]::ProcessorCount` in CI rather than
hardcoded. Note `-j 8` (5.4 min) is barely better than `-j 4` (5.8), so the
debugger serialises somewhere and a hosted runner loses little to a development
machine.

**It should have been the first thing tried, not the last.** Two rounds of
picking at test workloads came first and were worth about a third of what one
flag was. Everything else here is the record of that detour, kept because two of
the attempts made things worse and the failures are the useful part.

### The thing that mattered most: the run was not repeatable at all

Savers that use `rsMath`'s generator draw from one seeded by `std::random_device`
since rslibs L4. **No two runs did the same work**, and the cost is unbounded
rather than merely noisy: `skyrocket` picks a sucker-and-shockwave or
stretcher-and-bigmama explosion on `if(!rsRandi(2500))` (`skyrocket.cpp:702`),
each spawning enormous numbers of particles.

Measured on one commit with one command: **40 minutes, then 358 minutes.**
Coverage moved between runs for the same reason, which means every per-module
figure taken before this was a sample, not a measurement.

Suites now seed it to `kTestSeed` themselves. Change that value only
deliberately — the point is that a timing or coverage change means the code
changed rather than the dice.

**Only six savers can be seeded this way**, and the seeding cannot live in the
shared fixture. Seven others carry private copies of `rsRandi`/`rsRandf` — and
`starfield` its own `rsRandGen` — so including `<rsMath/rsMath.h>` in the common
header put two definitions of the same inline function in one binary and crashed
`starfield` in Release while Debug stayed green. That is Task 12, and this is
what raised it from tidiness to undefined behaviour. Six of the seven are on
plain `rand()`, so they are not seedable at all until it is fixed.

### What else worked: stop doing pointless work

Worth roughly a third of what `-j` was, and every one of these reduced how many
times a path repeats rather than which paths run — so coverage held. The
settings live in each suite's fixture, and every default is still asserted by
that suite's `SaverBodyWasActuallyCompiled` test, which is a plain `TEST` and so
never sees the fixture.

| Suite | Change | Effect |
|---|---|---|
| `euphoria` | `dDensity` 35 → 12 | 121 s → 9.6 s; the wisp mesh is `(dDensity + 1)²` per wisp, so this falls away quadratically |
| `lattice` | `dDepth` 5 → 2 | cells tested per frame 3,375 → 729 |
| `fieldlines` | `dMaxSteps` 300 → 100 | 54 s → 23 s |
| `hyperspace` | `numAnimTexFrames` 20 → 16 | almost nothing, ~1%; 16 is the floor anyway, since `causticTextures` clamps to `numKeys * 2` |

- `hyperspace` built its caustic texture set in all 15 cases although only two
  are about tunnels. Gating it on `dUseTunnels` in the fixture: suite **13.6 s →
  4.1 s** uninstrumented, and it was the single largest suite in the instrumented
  run.
- `skyrocket` pumped 120 frames in each of two tests with a **frozen clock** —
  see the `frameTime` trap below — so they redrew one static instant 120 times.
  Driving `frameTime` and cutting the loops: suite **17.5 s → 8.1 s**, and
  coverage went *up*, `particle.cpp` 11.7% → 17.2%.

### What did not work

| Attempt | Result |
|---|---|
| run the 15 binaries directly, not `ctest`'s ~300 processes | 40.3 min vs 40.0, and **0.4 points less** coverage |
| add `--modules` to restrict instrumentation to the test binaries | **far slower**: `fieldlines` went 53 s → 2,013 s |

The direct-binary route loses coverage because a single-process run lets
history-dependent statics carry between cases; `soundEngine.cpp` drops
74% → 51% and `helios.cpp` 80% → 73%. **Keep the `ctest` form** — per-test
isolation is load-bearing, not incidental.

`--modules` is the one that looks most like an obvious win and is the worst.
OpenCppCoverage defaults it to `*` and genuinely does select every Windows DLL,
so restricting it reads as pure hygiene. Measured, it is a 38× pessimisation on
at least one suite. Do not re-add it without measuring.

> **A warning about measuring this.** `ctest`'s `LastTest.log` is overwritten by
> every run, instrumented or not, and `CTestCostData.txt` is a rolling average
> across both — so it is very easy to attribute instrumented time using numbers
> from a plain run. Worse, the file's per-test blocks do not line up one-to-one
> with `Test time =` lines, so pasting the two lists together silently
> misattributes everything. An earlier version of this section did exactly that
> and named the wrong suites. Parse it by tracking the current test name, and
> check the total is plausible before trusting the breakdown.

If this needs to get faster still, the lever is the work itself, not the harness.
`euphoria` is the next largest suite and nothing has been done to it; the two
patterns that paid off above are the ones to reach for — `doingPreview`, which
only `hyperspace` and `microcosm` currently use, and checking whether a loop is
simulating anything at all before assuming its length is earning something.

---

# Status at a glance

| # | Task | State |
|---|---|---|
| 1 | Post-build copy with spaced paths | **done** — PR #38 |
| 2 | Debug build warnings | **partial** — `EditAndContinue` done in #39; `MinimalRebuild` and Debug LTCG remain |
| 3 | Declare C++17 explicitly | open — 13 of 14 projects |
| 4 | `resource.h` include style | **done** — PR #37 |
| 5 | `aboutProc` truncates an `HBRUSH` | **done** — PR #39 |
| 6 | Encapsulate mutable module globals | open |
| 7 | `libs` submodule | **done** — rslibs L1–L5, L8; bumped in #40 |
| 8 | SonarCloud duplication | **done** — 1.6% project-wide, under the 3% threshold |
| 9 | C++20 | open, blocked on 3 |
| 10 | Reliability bugs | **partial** — the two cyclone BLOCKERs are proven unreachable; 10 bugs remain |
| 11 | Registry values used unclamped | open |
| **12** | **Seven savers carry private PRNG copies — an ODR violation that crashes Release** | open, **raised** |
| 13 | Clear-text `http://` URLs | open |
| 14 | `solarWinds` sets `readyToDraw = 1` on `WM_DESTROY` | **done** — PR #44 |
| 15 | `fieldlines` nests `glBegin`, silently losing its line widths | open |
| 16 | Destructors sized from live globals | **done** |
| 17 | Test coverage rollout — all 13 savers | **done** |
| **18** | **`hyperspace` drew through freed texture objects after teardown** | **new, done** |
| **19** | **`skyrocket` indexed an emptied particle vector after teardown** | **new, done** |
| **20** | **`helios` keeps two restart-unsafe statics, one an out-of-bounds read** | **new, done** |
| **21** | **`hyperspace` calls `glActiveTextureARB` with no extension check** | **new, open** |
| **22** | **`lattice` disabled texture generation it never enabled** | **new, done** |
| **23** | **`lattice` carried a dead frustum-culling block with two invalid GL calls** | **new, done** |
| **24** | **Three savers deviate from the entry-point contract** | **new, open** |
| **25** | **`skyrocket` will not start without `OpenAL32.dll`** | **new, open** |
| **26** | **`microcosm` appends 55 gizmos on every `initSaver`; the clear is commented out** | **new, open** |
| **27** | **`skyrocket` has two more restart-unsafe statics (dangling `soundengine`, stale `rocketTimeConst`)** | **new, open** |

Tasks 10–13 came out of the rslibs work and a re-read of SonarCloud. Tasks 14–16
came out of the first two harness batches (PRs #42, #43) and Tasks 18–25 out of
the last one. Task 27 came out of implementing Task 10 itself — the same
restart-safety pattern turning up again while writing tests that call
`skyrocket`'s particle functions directly. Every one is a real defect that
thirteen years of running the
savers never surfaced, and each that is still open is pinned by a test asserting
**current** behaviour, so the fix has a tripwire. The test says so in its own
comment.

---

# Priority and order

**Ordering principle: things that can corrupt memory first, then the cheap
mechanical work that clears warning noise hiding them, then robustness against
bad input, and ratings-only refactors last.** The current quality gate cannot be
the guide here — see the note at the end of this section.

## First — correctness

1. **Task 10, what remains.** The two `cyclone` BLOCKERs are **resolved** — see
   below — so what is left is `lattice.cpp:703` (uninitialised field) and the
   six `cpp:S836` findings in `skyrocket/particle.cpp`, all "garbage value
   returned to caller". `skyrocket` now has a test binary, but `particle.cpp`
   itself is only 12% covered, so **extend that suite before touching these** —
   see the note under Task 10.
2. **Task 21.** `hyperspace` calls through a null function pointer on its first
   frame if the ARB extensions are absent. Unreachable on any GPU made this
   century, so it ranks below the reads above — but it is a crash, the fix is to
   move three lines inside an `if` that already exists, and until it is done the
   no-extension path cannot be tested at all.
3. **Task 15.** `fieldlines` loses its per-segment line widths to nested
   `glBegin` calls. Visible only as a subtly wrong render, so lower than the
   memory bugs, but it is a genuine rendering defect rather than a lint.
4. **Task 25.** `skyrocket` will not start on a machine without OpenAL
   installed. Not a memory bug, but it is the only item on this list that makes a
   saver completely unusable for a real user, and shipping the redistributable is
   a packaging change rather than a code one.

## Second — cheap, mechanical, high value per minute

4. **Task 2 remainder.** Delete `MinimalRebuild` from 13 projects and the Debug
   `LinkTimeCodeGeneration` from 9. This is a one-line-per-file substitution that
   removes **all 44 remaining Debug warnings** (26 `D9035`, 18 `LNK4075`). Doing
   it early means every later task's build output is readable.
5. **Task 3.** Add `<LanguageStandard>stdcpp17</LanguageStandard>` to both
   configurations of 13 projects. Trivial, and the submodule is already there, so
   this ends the split where the libs compile as C++17 inside this very solution
   and the savers do not.
6. **Task 13.** 26 `http://` URLs across `.cpp` and `.rc`. A one-line change that
   clears 6 `cpp:S5332` findings, and these are live `ShellExecute` calls that
   open a browser, so it is a real if small user-facing fix.

## Third — robustness against untrusted input

7. **Task 11.** ~124 registry values assigned with no bounds check; exactly one
   is clamped today. This is the `src/` counterpart of the rslibs L5 work, and
   likely the root cause behind some of Task 10's findings, which is why it
   follows rather than leads them.
8. **Task 12.** Delete the six private `rand()` copies. Small, and it doubles as
   the first slice of Task 8.

## Alongside — keep the net growing

**Task 17**, the test rollout, is not sequenced against the rest: it is what
makes the rest safe to do. Every task above touches code that had no tests
until PR #42. Prefer covering a saver before changing it — that is how Tasks
14, 15 and 16 were found in the first place.

## Last — ratings and refactors

**Task 6**, then **Task 9**. Task 6 affects only the maintainability rating,
which is the one rating still at A. Task 9 needs 3 first.

## Do not steer by the quality gate

Two of these numbers moved since the last revision, one of them a lot:

| Metric on `main` | Value | Reading |
|---|---|---|
| `reliability_rating` | **E** | Real, and now down to **10 open bugs** from 20. Task 10. |
| `security_rating` | **C** | 17 vulnerabilities: 11 `cpp:S2245` (PRNG) and 6 `cpp:S5332` (clear-text URL). S2245 fires on **every** PRNG including `<random>`, so Task 12 will not clear it — it was accepted as *Safe* in rslibs and the same is appropriate here. |
| `duplicated_lines_density` | **1.6%** | Was 6.2%. Now comfortably under the 3% threshold, so **Task 8 is done**. |
| `sqale_rating` | **A** | Maintainability, unchanged. |

Two things account for the bug count halving without anyone fixing bugs:
`ncloc` fell from 192,455 to 163,613 because **the analysis no longer scans the
`libs` submodule**, which removes the seven `Implicit` findings this document
used to list under Task 10; and the two `cyclone` BLOCKERs, while still
reported, are now demonstrated unreachable.

**Earlier revisions of this document claimed security, reliability and
maintainability were all A.** They were not, and the correction stands: only
maintainability is A.

---

# P0 — Actually breaks builds

## Task 1 · Post-build copy fails on paths with spaces — DONE (#38)

Resolved. Every `.vcxproj` now uses:

```xml
<Command>copy /Y "$(OutDir)$(TargetName)$(TargetExt)" "$(OutDir)$(TargetName).scr"</Command>
```

Kept here for the reasoning, which still applies to any new post-build event: a
post-build event runs under `cmd.exe`, so `powershell Copy-Item ...` passes
through **two parsers**. Double quotes plus `-LiteralPath` still breaks, because
`cmd` strips the quotes; single quotes break on any path containing an
apostrophe.

---

# P1 — Cheap, mechanical, low risk

## Task 2 · Debug build warnings — PARTIAL

`EditAndContinue` → `ProgramDatabase` landed in #39, which cleared every
`LNK4075 '/EDITANDCONTINUE'`. **Two of the three changes remain**, both Debug
configuration only — never touch Release:

| Change | Files | Clears |
|---|---|---|
| Delete `<MinimalRebuild>true</MinimalRebuild>` | 13 | 26 × `D9035` |
| Delete `<LinkTimeCodeGeneration>` from the **Debug** `<Link>` group | 9 | 18 × `LNK4075 '/INCREMENTAL'` |

```bash
grep -rl "MinimalRebuild" src --include=*.vcxproj
```

The 9 with Debug LTCG: `cyclone flocks helios hyperspace lattice microcosm
plasma skyrocket solarwinds`.

**Why:**

- `/Gm` (`MinimalRebuild`) is deprecated and raises `D9035`. It only drove an
  incremental-compile heuristic and is mutually exclusive with `/MP`, so
  removing it changes no output.
- LTCG in Debug has no `/GL` objects to act on — Debug sets no
  `WholeProgramOptimization` — so it does nothing while disabling the
  incremental linking that the same configuration asks for. **Keep** the Release
  `<LinkTimeCodeGeneration>`, where it is correctly paired with
  `WholeProgramOptimization`.

Together these take a Debug rebuild from 44 warnings to **0**.

## Task 3 · Declare C++17 explicitly

**13 `.vcxproj`** — every one except `starfield`. Add to the `<ClCompile>` group
of **both** configurations:

```xml
<LanguageStandard>stdcpp17</LanguageStandard>
```

```bash
grep -rL "LanguageStandard" src/*/*.vcxproj
```

**Why:** no saver project declares a standard, so they land on the MSVC default
(C++14), while `tests/` compiles as C++17 because GoogleTest forces it. The
submodule now declares C++17 explicitly in all three of its build systems
(rslibs L3), and its projects are built *by this solution* — so today one
solution compiles its libraries as C++17 and its executables as C++14.

Set both configurations together. Debug and Release compiling different language
versions is a worse failure mode than the consistent implicit default they have
now.

**Risk is low.** A scan for every construct C++17 *removed* found zero hits
across `src/` and `libs/`: `register`, `auto_ptr`, `random_shuffle`, `bind1st`,
`bind2nd`, `throw()`, `unary_function`, `binary_function`, `ptr_fun`, `mem_fun`.

## Task 4 · `resource.h` include style — DONE (#37)

## Task 5 · `aboutProc` truncates an `HBRUSH` — DONE (#39)

All 12 are now `INT_PTR CALLBACK`, returning `(INT_PTR)GetSysColorBrush(...)`,
with the `DLGPROC(...)` casts dropped. `screenSaverConfigureDialog` changed in
the same PR, in lockstep with rslibs L2.

---

# P0 (new) — Reliability

## Task 10 · SonarCloud bugs — PARTIAL

Not in the original brief, because this document claimed reliability was **A**.
It is **E**. The count is now **10 open bugs**, down from the 20 recorded here
before — but almost none of that is fixes:

- **Seven went away with the analysis scope.** `ncloc` fell from 192,455 to
  163,613 because Sonar no longer scans the `libs` submodule, taking the whole
  `Implicit` group with it. Those findings still exist; they are simply
  rslibs' problem now and are not visible from this project.
- **Two are resolved on the evidence**, see below.

### The two cyclone BLOCKERs are unreachable — RESOLVED

| Where | Rule | Finding |
|---|---|---|
| `cyclone.cpp:155` | `cpp:S3519` | Access of the heap area at negative byte offset -8 |
| `cyclone.cpp:163` | `cpp:S3519` | Access of `float *` element in the heap area at index 2 |

Both sit in `initSaver`, where `xyz` is allocated as `new float*[dComplexity+3]`
and indexed from `dComplexity+2` downwards. Reaching either needs a **negative
`dComplexity`**, and that cannot happen: `screenSaverProc` calls `readRegistry()`
before `initSaver()`, and `readRegistry` clamps to 1..10 **unconditionally** —
the clamp sits outside the `RegQueryValueEx` success check, so it applies to the
default value too (`cyclone.cpp:646`, added by #35).

Three `CycloneBlockerGuard` tests in `tests/test_cyclone.cpp` pin that reasoning,
including the ordering. **They still report as BLOCKERs in SonarCloud** — the
analyser cannot see the clamp across functions — so either mark them *Safe*
there or leave them with this note attached. Do not "fix" them by adding casts.

> One caveat, written up in full in the test file: `readRegistry` returns early
> when the saver has never stored settings, which is the case on a fresh CI
> runner. There the clamp lines never execute and the guard only confirms that
> `setDefaults`' value survives. Task 11 is what would make it unconditional.

### Four findings — RESOLVED

| Doc said | Actually was / is now | Rule | Finding |
|---|---|---|---|
| `lattice.cpp:703` | `lattice.cpp:700` (Task 23 removed 6 lines above it, Task 22 added 3 back) | `cpp:S2107` | `theCamera = new camera;` — root cause is `camera(){}` in `src/lattice/camera.h:36-38` leaving `farplane` and `cullVec[4][3]` indeterminate |
| `lattice.cpp:275` | no drift — above the Task 23 deletion at 488 | `cpp:S836` | `makeTorus`'s eight `old*` locals, written only on the `j==0` pass of a loop that never runs when `longitude <= 0`, read regardless and used to close a 2-vertex `GL_TRIANGLE_STRIP` |
| `skyrocket/particle.cpp:68, 844, 874, 902, 938` | `:49, 848, 878, 906, 942` — the four `pop*` lines drift by a consistent +4 (one `= nullptr` inserted ahead of each of the four functions); `:68` was already stale before this change, by more than the fix accounts for | `cpp:S836` | `randomColor`'s `i, j, k`, and four `pop*` functions' `newp`, read on paths no shipped call site can reach today |
| `cyclone.cpp:348` | no drift | `cpp:S2193` | a `float` loop counter accumulating `0.02f` ran a 51st time and drew a duplicate vertex |

`lattice.cpp:703` (`cpp:S2107`, uninitialised field) was confirmed **not** the
dead culling block removed under Task 23 — that was `lattice.cpp:488`, in
`draw()` rather than a constructor — bearing out the warning already in this
section to re-check line numbers before starting.

**`camera`'s constructor** (`src/lattice/camera.h`) now gives `farplane` and
`cullVec` default member initialisers. `init()` still sets both before
anything reads them; the finding was about the constructor leaving them
indeterminate, not a live read of garbage.

**`makeTorus`** gains a precondition guard —
`if(longitude < 1 || latitude < 1) return;` — ahead of the division by
`longitude` a few lines later, plus `= 0.0f` initialisers on the eight
`old*` locals so a later edit to the guard cannot reintroduce the read.
`dLongitude` comes from the registry unclamped (Task 11), so zero is
reachable in practice — unlike the two `skyrocket` findings below.

**`particle.cpp:49` and the four `pop*` lines are not reachable from any
shipped call site**, and were fixed anyway:

- `randomColor`'s `switch(rsRandi(6))` covers every value `rsRandi(6)` can
  return (`[0, 6)`), so `i`, `j` and `k` were always assigned before use. The
  `default:` arm a routine fix would add is itself unreachable code, trading
  this finding for the open `cpp:S1763` — initialised (`= 0, = 1, = 2`)
  instead.
- `popSphere`, `popSplitSphere`, `popMultiColorSphere` and `popRing` left
  `newp` unset when `numParts` is not positive, then dereferenced it one time
  in a hundred through the post-loop long-life branch. Every shipped call
  site passes a positive constant. Fixed with `= nullptr` and
  `if(newp && !rsRandi(100))` — null checked first, so the generator is still
  drawn from exactly once per call on the normal path and the RNG stream is
  unchanged.

Both were fixed with a test guarding the reachable behaviour rather than a
crash reproduction, since neither is reachable today: the switch-totality
guard (`Skyrocket.RandomColourAlwaysSetsExactlyOneChannelFull`) and the
long-life-branch guard (`Skyrocket.PopFunctionsStillGrantTheOccasionalLongLife`).

**`particle.cpp` coverage went from 17.6% (203/1154) to 95.3% (1104/1159)**,
by driving the sixteen particle types and seventeen `draw()` arms directly
through their own initialisers instead of waiting for the simulation to reach
them — `tests/test_skyrocket.cpp`'s `EveryExplosionTypeSpawnsTheParticlesItPromises`
and `EveryParticleTypeUpdatesAndDrawsCoherently`. `initShockwave` and
`initBigmama` in particular are otherwise reached only through a 1-in-2500
branch (`skyrocket.cpp:702`). This is the coverage work the previous revision
of this section said had to come before the `cpp:S836` fixes; it has now been
done, and the four dereferences above went in guarded by it.

**`cyclone.cpp:348`** was `for(step=0.0; step<1.0; step+=0.02f)`. Accumulated
in `float`, `step` stood at about 0.9999996 after fifty additions — still
`< 1.0` — so the loop ran a 51st time and drew a duplicate vertex at the end
of the "Show Curves" overlay curve. Replaced with an integer counter sampling
exactly 50 points. The rule title for `cpp:S2193` could not be confirmed
without network access or a checked-in Sonar report; the fix also removes the
`0.0`/`1.0` double literals compared against a `float` loop variable, which
covers every candidate reading of the rule.

Remaining lower-severity rules: `cpp:S6232` (type punning, ×4), `cpp:S1763`
(unreachable code, ×2), `cpp:S836` (garbage value returned, ×2).

---

# P1 (new) — Robustness

## Task 11 · Registry values are used unclamped

**13 savers, ~124 assignments, exactly one clamped.**

```bash
grep -rn "^\s*d[A-Za-z]* = val;" src/*/*.cpp | wc -l
```

Every saver's `readRegistry` does:

```cpp
result = RegQueryValueEx(skey, "Speed", 0, &valtype, (LPBYTE)&val, &valsize);
if(result == ERROR_SUCCESS)
    dSpeed = val;          // no bounds check, no valtype check
```

Counts per file: `skyrocket` 14, `flux` 13, `lattice` 12, `microcosm` 12,
`euphoria` 11, `flocks` 11, `hyperspace` 10, `solarWinds` 10, `helios` 9,
`cyclone` 8, `fieldlines` 8, `plasma` 5.

**Why it matters concretely:** `lattice` computes `rsRandf(150 - dSpeed)` and
`rsRandi(11 - dPathrand)` directly from these values. rslibs L4/L5 hardened the
library side — `rsRandi` no longer divides by zero and the frame rate limit is
clamped on read — but that only stops the crash; the setting is still garbage.

**Two patterns to copy, both already in the tree:**

- `cyclone.cpp:646` (from #35) — the minimal inline form:
  ```cpp
  if(dComplexity < 1) dComplexity = 1;
  if(dComplexity > 10) dComplexity = 10;
  ```
- `src/starfield/starfieldSettings.h` — the better form: a `windows.h`-free
  header holding the ranges, with `clampToRange(unsigned long, Range)` taking the
  value as `unsigned long` so an oversized `DWORD` is never converted to `int`
  first. Casting first turns `0xFFFFFFFF` into `-1` and slips past a naive
  lower-bound check. `rsWin32Saver/rsWin32SaverSettings.h` is the same idea in
  the submodule.

The header form is worth the extra effort: it makes the bounds testable without
an `HWND`, which is how rslibs got `rsWin32Saver` under test at all.

**One gap even in `starfield`:** its `readRegistry` assigns `dFrameRateLimit`
directly rather than going through `readFrameRateLimitFromRegistry`, so it does
not get L5's clamp. Fix that in the same pass.

**Two things the test work established.**

`starfield` is the working model and it is now proven, not just asserted:
`StarfieldFramework.ReadRegistryClampsEveryValueIntoRange` feeds it -1 and
100000 and checks every value lands inside the declared range. That test holds
**whether or not a registry key exists**, because the clamp runs on the
values `setDefaults` supplies too. No other saver can be tested that way.

That is also why this task blocks better testing elsewhere. Every other saver's
`readRegistry` returns early when the key is absent — the case on a fresh CI
runner — so on CI those lines never run at all. Concretely: coverage is about
five points lower on CI than on a developer machine that has run the savers, and
the `cyclone` BLOCKER guard (Task 10) only bites where a key exists. Giving each
saver a settings header with a pure clamp function makes both problems go away,
because the clamp becomes testable without a registry at all.

## Task 12 · Seven savers carry private PRNG copies — and it is an ODR violation

**Raised from tidiness. This is undefined behaviour that already changes
behaviour between Debug and Release**, demonstrated below, and it is seven
savers rather than six.

```bash
grep -rln "inline int rsRandi\|inline float rsRandf\|inline std::mt19937& rsRandGen" src --include=*.cpp
```

| Saver | Private copy |
|---|---|
| `cyclone`, `fieldlines`, `flocks`, `flux`, `plasma` | both `rsRandi` and `rsRandf`, on plain `rand()` |
| `solarwinds` | `rsRandf` only |
| **`starfield`** | **`rsRandf` and its own `rsRandGen`** (`starfield.cpp:79`) |

Each defines a function at global scope with the same name and signature as an
`inline` one in `rsMath.h`, but a **different body**. Put both in a program and
the One Definition Rule is broken: the linker keeps one COMDAT and discards the
other, and it does not have to choose the same way twice.

It is not theoretical. Adding `#include <rsMath/rsMath.h>` to the shared test
fixture — to seed the generator — was enough to make the two definitions meet:

- Debug: all 297 tests passed.
- Release: `starfield` **access-violated**, taking out all eight of its cases
  that call `initSaver`. Same source, same seed, same machine.

The crash path is worth knowing, because it survives whichever definition wins.
`starfield`'s private `rsRandf` is `uniform_real_distribution<float>(0.0f, x)`,
which is undefined for a negative `x` — precisely the bug rslibs L4 fixed in the
library version. A NaN out of it reaches:

```cpp
float size = float(dStarSize) * brightness;
if(size < 1.0f) size = 1.0f;        // false for NaN, so no clamp
auto bucket = int(size + 0.5f);     // INT_MIN
if(bucket > maxStarSize) ...        // false, so no clamp either
sizeBuckets[bucket].push_back(i);   // indexes far out of bounds
```

Both guards are comparisons that a NaN slips through. Worth a second look even
after the PRNG is unified.

Until it is fixed, **do not include `<rsMath/rsMath.h>` anywhere that links a
saver carrying a private copy** — `tests/support/saver_test_common.h` cannot,
which is why only the six clean savers can have a seeded generator.

Each is a verbatim duplicate of what `rsMath.h` used to contain, carrying the
same *"Don't forget to initialize with srand()"* comment. **rslibs L4 did not
reach them**, because they never include `rsMath.h` — contrary to what the
rslibs brief predicted.

Delete the copies and include `<rsMath/rsMath.h>` instead. That inherits the
thread-local Mersenne Twister, removes the modulo bias, and deletes six blocks of
duplicated code — a down payment on Task 8.

Note the `srand((unsigned)time(NULL))` calls in 13 savers become dead once this
lands; they are already dead for the seven modules that use the library version.

## Task 13 · Clear-text `http://` URLs

**26 occurrences** across `.cpp` and `.rc`, clearing 6 `cpp:S5332` findings.

```bash
grep -rn "http://" src --include=*.cpp --include=*.h --include=*.rc
```

These are not cosmetic: they are live `ShellExecute(NULL, "open", ...)` calls in
the About boxes plus the matching `CTEXT` labels. Both
`http://www.reallyslick.com` and `http://www.chromatek.com` (in `flocks`) should
be checked for a working `https://` before switching, and the `.rc` label must
change with the `.cpp` call so they do not disagree.

---

# P0 (new) — Found by the test harness

Three defects that thirteen years of running these savers never surfaced. Task
14 is fixed; the two that remain are each pinned by a test asserting the
**current** behaviour, so fixing one will make its test fail — which is the
point. The test says so in its own comment.

## Task 14 · `solarWinds` sets `readyToDraw = 1` on `WM_DESTROY` — DONE (#44)

`solarWinds.cpp:859` used to read:

```cpp
case WM_DESTROY:
    readyToDraw = 1;      // every other saver sets 0
    cleanUp(hwnd);
    break;
```

`cleanUp` then deletes the particle, emitter and wind arrays, and `idleProc`
guards drawing on exactly this flag. A frame arriving after `WM_DESTROY` draws
from freed memory.

It survives because `WM_DESTROY` is normally followed straight away by the
message loop ending and the process exiting, so nothing calls `idleProc` in
between. That makes it latent, not harmless: the guard does not do what the
identical line in the other twelve savers does.

Almost certainly a typo. **One character**, now `0`. The pinning test
`SolarWindsFramework.DestroyLeavesReadyToDrawSet` was folded back into
`SolarWindsFramework.ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy`,
matching the other suites. That assertion is the only coverage of the
`WM_DESTROY` arm: `SaverFixture::stop()` calls `cleanUp` directly, so no `TEST_F`
case reaches `screenSaverProc`.

Note this did **not** clear Task 16, which lives in the same file:
`wind::~wind` sized its frees from the live globals — fixed separately, see
below.

## Task 15 · `fieldlines` nests `glBegin` and loses its line widths

With `dConstwidth` false — the **default** — `drawfieldline` reopens a
`GL_LINE_STRIP` on every step so it can vary `glLineWidth` per segment
(`fieldlines.cpp:247-249`), but the matching `glEnd` only runs on the final step
(`fieldlines.cpp:258-260`). Every intermediate `glBegin` therefore lands inside
an already-open block.

A driver treats each as `GL_INVALID_OPERATION` and ignores it — along with the
`glLineWidth` calls between them. So **the per-segment width the code is
reaching for never applies**, and the line renders as one uniform strip. It
looks plausible, which is why it has survived; it is also spamming GL errors
every frame.

The fix is to close each strip before reopening it. Note that doing so changes
what the saver looks like, so it wants an eye on the result rather than just a
green build. Pinned by `Fieldlines.DefaultModeLeavesGlBeginBlocksUnclosed`.

## Task 16 · Destructors sized from live globals — DONE

`wind::~wind` in `solarWinds.cpp` used to free `particles[i]` for
`i < dParticles` — the global, not the count the object was constructed with:

```cpp
wind::~wind(){
    for(i=0; i<dEmitters; i++) delete[] emitters[i];
    ...
    for(i=0; i<dParticles; i++) delete[] particles[i];
```

Change a count between `initSaver` and `cleanUp` and the destructor walks off the
end of the array deleting garbage pointers. It blocks inside the heap; a test
that did this sat at **zero CPU for ten minutes** before the cause was
understood.

The saver cannot reach this itself — settings only change through the dialog,
which writes the registry, and `initSaver`/`cleanUp` bracket a whole run — so it
is a latent trap rather than a live bug. It is still worth fixing: an object
that remembers its own size is both correct and cheaper to reason about, and the
current shape silently constrains anything that wants to reconfigure a saver in
place.

`cyclone` and `flocks` were checked and are **not** affected: they free with
`delete[]`, which knows its own extent.

**Fixed:** `wind` now captures `numEmitters`, `numParticles` and
`hasLineList` at construction and the destructor frees by those instead of
the live globals. Pinned by
`SolarWinds.DestructorFreesWhatItAllocatedNotCurrentGlobals`, which raises
`dEmitters`/`dParticles` and flips `dGeometry` between `start()` and `stop()`
— exactly the mismatch the bug needed — and asserts teardown survives it.

---

# P0 (new) — Restarting a saver in the same process

Tasks 18, 19, 20, 26 and 27 are one defect wearing five hats, and Task 16 above
was the first sighting. **Teardown releases memory but leaves the things that index it
untouched** — counters, "already built" flags, function-local statics, and in one
case the container itself. The next `initSaver` starts from a clean allocation and
a dirty bookkeeping state.

None of it can happen in the shipped savers, where the process exits rather than
restarting. All of it happens immediately under a test fixture, which is why four
of these turned up in one afternoon. Worth knowing before writing any new
teardown code: **if `cleanUp` frees it, `cleanUp` owns resetting whatever counts
it.**

`microcosm`'s `cleanUp` is worth singling out: it frees **nothing**, and neither
does `helios`'s `textwriter` or `skyrocket`'s `theWorld`. Irrelevant at process
exit, but it means none of these savers has a working teardown to build on.

## Task 18 · `hyperspace` drew through freed texture objects — DONE

`draw()` built its caustic textures and wavy normal cube maps on the first frame,
guarded by a `static int first` inside the function, because they are rendered
into the framebuffer and read back. `cleanUp` deleted both. The flag stayed set,
so the next frame after a restart used the freed pointers.

Reliably an access violation, plus heap corruption from the second `delete`.

**Fixed:** the flag is now a file-scope `texturesBuilt`, cleared in both
`cleanUp` overloads alongside the deletes, and the two pointers are nulled.
Pinned by `Hyperspace.RestartingRebuildsTheGeneratedTextures`.

## Task 19 · `skyrocket` indexed an emptied particle vector — DONE

`cleanup()` called `particles.clear()` and left `last_particle`, `numRockets`,
`numFlares` and `zoomRocket` alone. `addParticle()` then did:

```cpp
if(last_particle < particles.size())   // 5 < 0 is false
    ++last_particle;
return &(particles[last_particle-1]);  // index 4 of an empty vector
```

The guard that would have grown the vector back cannot help either:

```cpp
if(particles.size() - int(last_particle) < 1000)
```

is unsigned arithmetic, so an emptied vector against a non-zero counter wraps to
about four billion rather than going negative, and the resize never fires. Two
bugs holding each other up.

**Fixed:** all four counters are reset in `cleanup()`. The unsigned comparison is
left as it is — correct once the counter is zero — but it is fragile and worth a
look if that code is touched.

## Task 20 · `helios` keeps two restart-unsafe statics — DONE

Both were function-local statics that survived `cleanUp`, and the second was an
out-of-bounds read rather than merely wrong output:

1. **`ionsReleased`** (was `helios.cpp:452`) counted how many ions had been let
   out and was never reset, while `doSaver` reallocates `ilist` to the current
   `dIons`. Restarting with a **smaller** `dIons` left the draw loop at
   `helios.cpp:629` walking past the end of the array.
2. **`points`** in `surfaceFunction` (was `helios.cpp:440`) was
   `static int points = dEmitters + dAttracters;`, initialised on the first call
   in the process and never updated, while `doSaver` sizes the `spheres` array
   from the current settings (`helios.cpp:863`). Restarting with fewer emitters
   left it summing `spheres` past the end — on **every sample of a 70×70×70
   volume**.

A test written to demonstrate the second **access-violated rather than failing an
assertion**, which is how it was confirmed. Deliberately triggering an
out-of-bounds read does not belong in CI, which is why the replacement test
below stays inside the fixed array bounds and pins the counter's value instead.

**Fixed:** both statics are now file-scope `int`s — `ionsReleased` and
`surfacePoints` — assigned where the arrays they index are allocated in
`doSaver`: `ionsReleased = 0` right after `ilist = new ion[dIons]`, and
`surfacePoints = dEmitters + dAttracters` right after
`spheres = new impSphere[dEmitters + dAttracters]`. `ionsReleased` is also
reset in `cleanUp`, since `delete[] ilist` there is unconditional; `surfacePoints`
is not, since `cleanUp` frees `spheres` only under `if(dSurface)` and has no
meaningful value to give it at teardown — `doSaver` alone is what keeps the
count and the array inseparable.

The old pinning test, `Helios.IonReleaseCountSurvivesARestart`, never actually
observed `ionsReleased`: its `EXPECT_GT(glCallList calls, 0)` was satisfied by
the un-reset `releaseTime` schedule regardless of whether the counter was reset,
so it passed identically before and after this fix. It is replaced by
`Helios.RestartResetsTheIonReleaseCount`, which drives both the first cycle and
the restart past the entire 120 s release schedule (`kReleaseEverythingStep`)
so the two paths are told apart by the exact ion count drawn, not by timing.
`Helios.SurfaceModeTakesTheSurfaceBranch` is strengthened to
`Helios.SurfaceModeBuildsAndDrawsTheMesh`, now asserting `glDrawElements` and
non-zero vertex counts rather than only the texture-generation branch, and a
new case, `Helios.RestartWithFewerSpheresRebuildsTheSurface`, restarts with
fewer emitters/attracters and asserts `surfacePoints` matches the newly
allocated count and that a mesh is still drawn. The `Helios` fixture now seeds
`rsRandGen()` to `kTestSeed` so these mesh assertions are a fixed outcome
rather than a probable one.

**What remains history-dependent in `helios`, for the next person here:**
`draw()`'s camera, colour, release-schedule (`releaseTime`) and
pattern-interpolation statics (`wait`, `preinterp`, `interpconst`, `newTarget`)
all still survive `cleanUp` and were deliberately left alone — see Risks in the
task's implementation notes. `releaseTime` in particular is what made the old
pinning test look like it worked: it is a float schedule rather than an index,
so a stale value can only shift release *timing*, bounded by the
`ionsReleased < dIons` guard, never cause an out-of-bounds access on its own.
Separately, `rsVec`'s default constructor leaves `v[]` uninitialised
(`libs/rsMath/rsVec.cpp:25`), so a restarted saver blends freshly allocated
emitters from indeterminate `oldpos`/`targetpos` until `setTargets` runs again
— which is why the new tests drive `frameTime` well past `preinterp`'s PI
threshold before asserting anything about the mesh.

## Task 26 · `microcosm` appends its gizmo list instead of rebuilding it — OPEN

`initSaver` pushes 55 gizmos onto `gizmos`, and the `clear()` that should come
first is inside the comment on the line above them:

```cpp
	// initialize gizmos	gizmos.clear();      // microcosm.cpp:979
	{ Metaballs* gizmo = new Metaballs(7);  gizmos.push_back(gizmo); }
```

A tab, not a newline, between the comment text and the statement. So it never
runs: a second `initSaver` leaves 110 entries, and `chooseGizmo` then picks at
random from the doubled range. `cleanUp` frees nothing at all, so the 55 original
`Gizmo` objects leak as well.

The last entry is an easter-egg gizmo that `chooseGizmo` deliberately withholds
unless `gTennisAvailable`, by dropping one off the top of its random range. With
the list doubled that guard still excludes only the *final* entry — so the first
copy's easter egg becomes reachable at random. That is the one visible
consequence beyond the leak.

Splitting the line is a one-character fix. Freeing the gizmos in `cleanUp` is the
larger half, since it frees nothing today. Pinned by
`Microcosm.GizmoListGrowsOnEveryRestart`.

## Task 27 · `skyrocket` has two more restart-unsafe statics — OPEN

Found while implementing Task 10, not fixed there — out of scope for a
SonarCloud-bug pass, and neither is reachable from the shipped saver, for the
same reason nothing in this section is: the process exits on `WM_DESTROY`
rather than restarting. Both only fire under a test fixture that calls
`stop()` then `start()` in the same process, and Task 10's new tests were
written to route around them rather than trip them. Recorded here as the
fifth sighting of this section's defect and left for whoever picks it up next.

1. **`soundengine`** (`skyrocket.cpp:980`) is deleted in `cleanup` but never
   set back to `NULL`. `initSaver`'s `if(dSound) soundengine = new
   SoundEngine(...)` (`skyrocket.cpp:956`) overwrites it before the shipped
   saver's `WM_CREATE` → `WM_DESTROY` cycle can expose the stale pointer. It
   fires from anything that calls into `particle.cpp`'s nine `if(soundengine)`
   checks (`particle.cpp:114, 136, 160, 371, 437, 526, 685, 817, 1037`, spread
   across several `init*` functions and one `pop*`) directly after a
   `cleanup()` in the same process without
   going through `initSaver` first — exactly what `tests/test_skyrocket.cpp`'s
   `Skyrocket` fixture does between cases. Task 10's new cases were ordered to
   stay above `DrivesTheSoundEngineWhenAsked` for this reason, with a comment
   on both sides of the boundary recording why.
2. **`rocketTimeConst`** (`skyrocket.cpp:680`) is a function-local static
   seeded once per process as `10.0f / float(dMaxrockets)`. If the first call
   ever made in the process finds `dMaxrockets == 0`, the static becomes
   `+Infinity` and stays that way until the periodic recompute at
   `skyrocket.cpp:689` next runs — every 20 to 50 seconds of simulated time,
   not on every frame. `skyrocket.cpp:747`'s `if(dMaxrockets) rocketTimer =
   rsRandf(rocketTimeConst);` means that stale `+Infinity` is read the moment
   `dMaxrockets` next becomes positive, if that happens before the recompute
   fires — feeding `rsRandf` an infinite range. Task 10's new tests avoid this
   by setting `dMaxrockets = 1` before ever calling `start()`, so the static is
   never seeded at zero in the first place.

Neither has a pinning test yet. Fixing (1) is a one-line `soundengine =
NULL;` after the `delete`, the same pattern Tasks 18–20 used. Fixing (2)
needs the same move Task 20 made for `helios`'s statics: promote it to a
file-scope variable assigned at a well-defined point rather than at
first-use, since a function-local static's initialiser runs exactly once no
matter how many times the saver restarts around it.

## Task 21 · `hyperspace` calls `glActiveTextureARB` unguarded — OPEN

`initSaver` degrades properly when the extensions are missing:

```cpp
if(!initExtensions())
    dShaders = 0;          // hyperspace.cpp:517
```

and every use of the ARB entry points sits inside `if(dShaders)` — except three:

```cpp
glActiveTextureARB(GL_TEXTURE2_ARB);   // hyperspace.cpp:231
glActiveTextureARB(GL_TEXTURE1_ARB);   // 233
glActiveTextureARB(GL_TEXTURE0_ARB);   // 235
```

Those are function pointers that `initExtensions` only fills in on success, so
without `GL_ARB_multitexture` the first frame calls through address zero. The
"graceful fallback" is a crash.

Unreachable in practice — no GPU since roughly 2002 lacks the three extensions
the loader asks for — which is why it has never been reported. The fix is to move
the three calls inside the existing `if(dShaders)`, or guard on the pointer.

**Consequence for the harness:** the GL stub therefore *advertises*
`GL_ARB_multitexture`, `GL_ARB_texture_cube_map` and `GL_ARB_shader_objects` and
resolves their entry points, rather than reporting none. That is the path real
hardware takes anyway, but it means the no-extension path is **untested** in both
`hyperspace` and `microcosm`. `microcosm` has a genuine non-shader fallback
(`microcosm.cpp:634`) and is covered for it by
`Microcosm.RendersWithoutShadersToo`; `hyperspace` cannot be until this is fixed.

## Task 22 · `lattice` disables texture generation it never enabled — DONE

The enable was conditional on a reflective texture:

```cpp
if(dTexture == 2 || dTexture == 3 || ... ){   // lattice.cpp:580
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
}
```

and the matching disable, thirty lines later, was not:

```cpp
glDisable(GL_TEXTURE_GEN_S);   // lattice.cpp:613
glDisable(GL_TEXTURE_GEN_T);
```

With any other texture the frame disabled something it never enabled. Entirely
harmless — disabling an already-disabled capability is a no-op in GL — and the
only reason it was written down is that it was the one exception to the
`NoEnableStateLeaked` frame invariant in the *disable* direction.

**Fixed:** `draw()` computes the predicate once into a local `envMapped` and
hangs both the enable and the disable off it. The original shape kept the same
five-way test in two places thirty lines apart, so a texture id added to one and
not the other would have brought the defect back silently; there is now one list.

The pinning test was **inverted rather than deleted**, against the advice in its
own comment. `Lattice.DisablesTextureCoordinateGenerationOnlyWhenItEnabledIt`
walks `dTexture` 0 to 8 — every value a frame can see, since 9 means "random"
and `initSaver` resolves it — and asserts the net is 0 for each with the enable
happening only for 2 to 6. Deleting it would have left the line unguarded.
`Lattice.LeaksNoOtherEnableState` now covers a plain texture as well as a
reflective one, and `Lattice.EnvironmentMappedTexturesTurnOnCoordinateGeneration`
was folded into the two of them, being wholly subsumed. The suite is 24 cases and
the tree 296 tests as a result; the two figures of 297 elsewhere in this document
are measurements of specific past runs and are left as recorded.

**The invariant was not tightened repo-wide, and cannot be.** `flux` is still an
exception in the other direction: it establishes its blend state at the top of
every frame and never disables it (`flux.cpp:445-462`), which is legal, and
`Flux.EnableStateIsTheSameEveryFrame` holds that instead. `NoEnableStateLeaked`
stays an opt-in per suite.

## Task 23 · `lattice` carried a dead frustum-culling block — DONE

`draw()` declared `cullQuat`, `cullMat`, `transMat` and `cull[5]` — commented
"storage for transformed culling vectors" — and read none of them. The culling
was never written. Two `glGetFloatv` calls existed only to fill two of the four,
and they were wrong as well:

```cpp
glGetFloatv(GL_MODELVIEW, cullMat);   // GL_MODELVIEW is the mode, 0x1700
```

`GL_MODELVIEW_MATRIX` (`0x0BA6`) is the query. A real driver raises
`GL_INVALID_ENUM` and leaves the buffer untouched, so the destinations held
uninitialised stack even before anyone read them.

**Fixed by deletion** — correcting the enum would have kept two pointless calls
per frame feeding variables nobody reads. Found by the stub, which records
readbacks handed an enum it cannot answer; `Lattice.ReadsBackNoInvalidEnums` is
the regression guard, and every other suite carries the same assertion.

---

# P2 (new) — Consistency, found while building the harness

## Task 24 · Three savers deviate from the entry-point contract — OPEN

Twelve savers expose `draw()`, `idleProc()`, `initSaver(HWND)`, `cleanUp(HWND)`
and `LONG screenSaverProc(...)`. Three do not, and each cost a link error to
discover:

| Saver | Deviation |
|---|---|
| `helios` | initialiser is `doSaver(HWND)` (`helios.cpp:747`); there is no `initSaver` at all |
| `skyrocket` | teardown is `cleanup(HWND)`, lowercase u (`skyrocket.cpp:964`) |
| `flux` | `LRESULT screenSaverProc` where the other twelve use `LONG` (`flux.cpp:1097`) |

`setDefaults` splits too, and less arbitrarily: `(int which)` in the six savers
with presets, `()` in the seven without.

Renaming the first two is a two-line change each plus their call sites in
`screenSaverProc`. `LRESULT` and `LONG` are the same type on Win32, so `flux` is
cosmetic — but the inconsistency is what makes the framework contract implicit
instead of declared, and it is the thing that would have to be settled first if
these savers ever got a shared header.

The harness works around all three with one-line shims in the test files rather
than touching saver source, so nothing depends on this being fixed.

## Task 25 · `skyrocket` will not start without `OpenAL32.dll` — OPEN

`skyrocket.vcxproj` links `OpenAL32.lib` as a normal import, so the DLL is a
load-time dependency of the executable. On a machine without OpenAL installed —
which is most of them now; it has not shipped with Windows and the repo carries
headers but no redistributable — `bin\skyrocket.exe` exits immediately with
`0xC0000135` (`STATUS_DLL_NOT_FOUND`) and no message.

Confirmed on the development machine while verifying this branch: `lattice` and
`hyperspace` both return `-1` from `/x` as they should, and `skyrocket` returns
`-1073741515`. Nothing in the repo's own code runs before the failure.

`dSound = 0` does not help — the dependency is in the import table, not in
whether a `SoundEngine` is constructed. Fixing it properly means loading
`OpenAL32.dll` with `LoadLibrary` and resolving the nineteen entry points at
runtime, degrading to silence when it is absent; the cheap alternative is to ship
the redistributable in `bin\`.

Not found by the harness — the tests link `tests/support/al_stub.cpp` instead and
so never touch the real DLL, which is exactly why this needed a manual run to
catch.

---

# P2 — Larger, needs judgement

## Task 6 · Encapsulate mutable module globals (SonarCloud `cpp:S5421`)

Wrap each module's private globals in a struct reached through a
function-local static:

```cpp
namespace {
struct SaverState {
    int readyToDraw = 0;
    float frameTime = 0.0f;
    float aspectRatio = 0.0f;
    /* ...module-specific state... */
};
SaverState& state(){ static SaverState s; return s; }
}
```

Hoist `auto& s = state();` once per function rather than calling `state()`
inside hot loops, so the thread-safe-static guard is not hit per iteration.

**Safe because** each saver is its own executable, so identically-named globals
in different modules never collide. The framework mandates exactly one global —
`extern LPCTSTR registryPath` in `libs/rsWin32Saver/rsWin32Saver.h`, already
exempt as pointer-to-const. Everything else those headers declare
(`isSuspended`, `kStatistics`, `dFrameRateLimit`, `checkingPassword`,
`xdisplay`, `xwindow`) is framework-owned — **do not move it**.

**Two real traps:**

- **Multi-TU modules.** `skyrocket` (`flare.h`, `particle.h`), `hyperspace`
  (`tunnel.h`) and `microcosm` genuinely share `frameTime` / `aspectRatio`
  *across* translation units. Those cannot become file-local; they need a
  shared accessor or must stay as they are. Single-`.cpp` modules such as
  `plasma` have no such constraint.
- **`#ifdef RS_XSCREENSAVER` blocks are invisible to a Windows build.** In
  `starfield` alone that was 8 of 97 access sites. There is **no working Linux
  build for `src/` in this repository** — seven directories carry a stale
  `Makefile` (`flocks`, `flux`, `hyperspace`, `implicitDemo`, `microcosm`,
  `plasma`, `solarwinds`) but nothing builds them, and there is no CMake — so
  those branches are never compiled by anyone, by CI or locally. Review them by
  eye; a green Windows build proves nothing about them.

**Scale:** roughly 97 access sites in `starfield`; expect more in the larger
modules.

**Priority note:** this affects only the maintainability rating, which is the one
rating still at **A**. It clears no failing gate condition. Ranked last for that
reason.

**What the test work added.** The saver test suites have to declare these
globals to reach them (`extern int dFollowers;` and so on), and Sonar counts
each declaration as a mutable global too — currently 17 findings across the
suites and `tests/support/saver_shim.cpp`. Two things were established:

- **Hiding the declaration in block scope does not help.** Wrapping each in an
  accessor — `static int& svFollowers() { extern int dFollowers; return dFollowers; }` —
  was tried and reported exactly the same count, so it was reverted. Do not
  repeat it.
- **The shim's eleven are unavoidable.** `rsWin32Saver.h` declares them `extern`
  and the savers write to them, so `saver_shim.cpp` must *define* them at
  namespace scope with those names or nothing links.

Doing this task properly is therefore the only thing that clears them, which is
another small argument for it beyond the rating.

**After the full rollout that count is 79**, across the thirteen suites, and
they are deliberately **left open**. They are `extern` *declarations*; the tests
cannot make the savers' globals const, and the two workarounds above are already
ruled out by measurement. No suppression file, no `NOSONAR`, no rule exclusion —
the number honestly reports that the savers expose their settings as mutable
globals, and it drops to zero when this task lands.

One thing to keep true: **`src/` should gain no new ones.** PR #46 briefly added
a single global to `hyperspace` and it was removed again in `7e36902` by keying
off pointers that already carried the state.

## Task 7 · `libs` submodule — DONE

rslibs L1 (`EditAndContinue`), L2 (`DLGPROC` signature), L3 (C++17), L4 (PRNG),
L5 (frame rate clamp) and the `rsWin32Saver` half of L8 are merged; the pointer
is at `42d251b` via #40.

**Still open in rslibs**, both breaking and needing coordinated edits here:

- **L6** — redesign `initFrameRateLimitSlider` / `updateFrameRateLimitSlider` so
  all 13 savers can adopt the checkbox-plus-FPS control `starfield` implements
  locally. Note L5 already moved the `0..1000` bounds into shared constants, so
  the groundwork exists.
- **L7** — remove the global `to_string` template from `rsText.h`. Every
  unqualified call site here must become `std::to_string` in the same step.

Also **not** done: the `rsText` and `rsXScreenSaver` halves of L8. Those were
deliberately skipped — `rsText`'s only non-OpenGL surface is the `to_string`
template L7 deletes, and `rsXScreenSaver` is Xlib plumbing with nothing
decidable to test.

## Task 8 · SonarCloud duplication — DONE

Project-wide duplication is **1.6%**, comfortably under the 3% threshold. It was
25.2% when this document was first written and 6.2% at the last revision.

The saver boilerplate it describes — the FPS counter block, `readRegistry` /
`writeRegistry`, the dialog procedure — is still duplicated across the 13
modules. What changed is the denominator, not the numerator: `ncloc` fell when
the analysis stopped scanning `libs`. So **extracting a shared saver skeleton
remains worth doing** on its own merits (it would largely resolve Task 6 and
absorb Task 12), it just no longer has a failing gate behind it.

One live lesson from PR #43: the *test* suites hit this gate for real at 6.9%,
because six suites repeated the same fixture and the same frame invariants. The
fix was `tests/support/saver_test_common.h`, which cut 260 lines. Any new saver
suite should use it rather than copy an existing file — with seven savers still
to add, copying would put the gate straight back into failure.

## Task 17 · Test coverage rollout — DONE

Before PR #42 nothing in `src/` was tested. **All thirteen savers now are**, at
297 tests across fifteen binaries.

Measured locally at the end of the rollout. Each figure is for the saver's own
translation unit; several savers link many more, and those are listed separately
below — a saver spread over ten files says nothing useful as a single number.

| Saver | Coverage | Batch |
|---|---:|---|
| `plasma` | 88.4% | #42 |
| `cyclone` | 85.5% | #42 |
| `solarWinds` | 85.2% | #43 |
| `fieldlines` | 82.8% | #43 |
| `starfield` | 80.2% | #43 |
| `helios` | 80.2% | final |
| `flux` | 79.5% | final |
| `flocks` | 76.8% | #42 |
| `euphoria` | 75.3% | final |
| `lattice` | 74.7% | final |
| `hyperspace` | 74.6% | final |
| `microcosm` | 68.5% | final |
| `skyrocket` | 47.1% | final |
| **Whole of `src/`** | **71.9%** | 9,083 of 12,631 lines, target 60% |

The supporting units, which carry much of the real geometry:

| Unit | Coverage | Belongs to |
|---|---:|---|
| `world.cpp` | 99.3% | `skyrocket` |
| `mirrorBox.cpp`, `texture1d.cpp`, `rsCamera.cpp` | 98–99% | `microcosm` |
| `goo.cpp`, `wavyNormalCubeMaps.cpp`, `stretchedParticle.cpp` | 98–100% | `hyperspace` |
| `camera.cpp` | 100% | `lattice` |
| every gizmo header | 66–100% | `microcosm` |
| `smoke.cpp` | 94.5% | `skyrocket` |
| `splinePath.cpp`, `tunnel.cpp` | 81–86% | `hyperspace` |
| `soundEngine.cpp` | 74.1% | `skyrocket` |
| `causticTextures.cpp` | 63.7% | `hyperspace` |
| `flare.cpp` ×2, `starBurst.cpp`, `shockwave.cpp` | 38–52% | `hyperspace`, `skyrocket` |
| `particle.cpp` | **11.7%** | `skyrocket` |

**Read the two low numbers before drawing conclusions from the total.**
`skyrocket.cpp` at 47.1% and `particle.cpp` at 11.7% are 2,011 lines between
them and cost the overall figure roughly six points. Both have the same shape: a
switch over a dozen rocket and particle types that each fire only under their own
random conditions, so pumping frames reaches very few of them. Driving those
types directly is the highest-value work left here — and Task 10's remaining
findings live in exactly that file.

The total is **lower than the 77.3% this section used to record, and nothing
regressed.** The six savers measured then were the small ones; adding
`skyrocket`, `hyperspace`, `microcosm` and `lattice` roughly doubled the
denominator.

Coverage reads about five points higher locally than in CI, for the registry
reason under Task 11, so expect roughly 67% there. A CI job posts a
self-updating per-module table on every PR; it is **reported, not gated**,
because a threshold would have failed every PR that added a saver before its
tests. With the rollout finished that argument no longer holds, and gating at
the 60% the target already names is now worth considering.

### `implicitDemo` is excluded, deliberately

It is a freeglut demo, not a saver: `int main(int argc, char** argv)` calling
`glutInit`/`glutMainLoop` (`implicitDemo.cpp:267`), with `display()`/`reshape()`
instead of `draw()`/`initSaver()`, no registry and no dialogs. Testing it means
defining `main` away and adding a glut stub to reach 293 lines that are mostly
window plumbing, and its real payload is in `libs/Implicit` — rslibs, not `src/`.
The rollout is complete at **13 savers**, not 14.

### Two predictions from the plan that were wrong

Both were recorded here as blockers and neither survived contact:

- **"`lattice`, `hyperspace` and `skyrocket` need their projection maths
  extracted into pure functions."** They needed a matrix stack in the stub
  instead — three real 4×4 stacks with the fixed-function operations and coherent
  `glGetFloatv`/`glGetDoublev`/`gluProject`, about 250 lines of well-understood
  arithmetic. Extraction would have meant refactoring shipped rendering code,
  which is the larger change, not the smaller one. `tests/test_gl_stub.cpp` checks
  the stack against hand-worked values, because a transposed multiply there would
  fail no saver test while quietly invalidating every assertion built on it.
- **"`microcosm` starts two worker threads, and a test reaching the thread-start
  path hangs."** True, and irrelevant: `gUseThreads` (`microcosm.cpp:163`) is an
  ordinary global, and setting it `false` before `initSaver` selects a
  single-threaded branch (`microcosm.cpp:592`) that computes the surfaces inline.
  That branch is a **complete implementation**, not a fallback stub, and it is the
  more deterministic of the two — the threaded path draws a frame behind. The
  saver budgeted as hardest was an ordinary batch member.

`skyrocket`'s sound turned out the same way: `dSound` gates the `SoundEngine`
entirely (`skyrocket.cpp:955`), so most cases never reach OpenAL, and the
nineteen entry points `soundEngine.cpp` links against are trivial to stub.

### Traps worth knowing before adding a saver

These cost real time to find:

- **`WIN32` is a project define, not a compiler builtin.** MSVC predefines
  `_WIN32` only, and every saver body sits inside `#ifdef WIN32`. Compile one
  from CMake without it and the translation unit is empty — it links clean and
  covers nothing. Each suite has a `SaverBodyWasActuallyCompiled` test that
  fails loudly if the define is ever dropped.
- **`<gl/GL.h>` declares its entry points `WINGDIAPI`**, which is
  `__declspec(dllimport)`, and a dllimport function cannot be defined. `_GDI32_`
  is the switch `wingdi.h` itself uses to turn that off, which is what lets the
  stub satisfy the calls. `opengl32.lib` and `glu32.lib` are never linked.
- **One test executable per saver.** Every module defines `draw()`, `idleProc()`,
  `setDefaults()` and friends at global scope, so two savers cannot share a
  binary.
- **`--sources` must be an absolute path** when running OpenCppCoverage. The
  MSVC CRT's own sources live under paths containing `\src\`, so a bare filter
  drags in the whole runtime and reports about 11% instead of 77%.
- **Change a setting only while nothing is allocated** — see Task 16.
- **`AL_BUILD_LIBRARY` is the `_GDI32_` of OpenAL.** `3rdparty/openal/include/al.h`
  declares every entry point `__declspec(dllimport)` without it, and a dllimport
  function cannot be defined, so the stub satisfies nothing. Same trap, different
  header.
- **Three savers deviate from the entry-point contract** — `helios` has no
  `initSaver`, `skyrocket` spells teardown `cleanup`, `flux` returns `LRESULT`.
  See Task 24. Each is a link error, and each is worked around with a one-line
  shim in the test file rather than by touching saver source.
- **Match `<StackReserveSize>` if a project sets one.** `skyrocket` links with a
  10MB stack and needs it: against the linker's 1MB default, setup overflows the
  stack and the process dies with `0xC00000FD` before gtest reports anything.
  `implicitDemo` asks for 1GB, for whatever reason.
- **`frameTime` is exactly zero under test unless a test sets it — so a loop of
  `draw()` calls simulates nothing.** Every saver *spends* `frameTime` but only
  `idleProc` ever sets it, from an `rsTimer` tick. The suites call `draw()`
  directly, so the clock never starts and each frame redraws one frozen instant.

  This is the most expensive trap in the harness, because it fails silently and
  looks like thoroughness. `skyrocket` had two tests pumping 120 frames each to
  reach "explosions, smoke and shockwaves"; instrumenting them showed every frame
  emitting an identical 9,940 vertices, no rocket ever launching, and the pair
  costing **46% of the whole coverage run** for nothing. `helios` had a test
  asserting `EXPECT_GE(later, early)`, which a frozen clock satisfies by leaving
  both counts equal.

  Drive it: `extern float frameTime;` then set it before each `draw()`. Assert
  that something actually moved — `last_particle > 0`, a strictly greater count —
  so the guard fails loudly if the clock stops again. Match the step to the
  saver's own timescale: 1/30 s suits `skyrocket`, but `helios` spreads ion
  release over two minutes and needs half-second steps to get anywhere.

  `microcosm` is the exception that still wants its state set directly:
  `gModeTransition` gates which surface function is chosen, and forcing it to 1.0
  is clearer than simulating the ramp.
- **`doingPreview` is a legitimate speed switch.** `hyperspace` and `microcosm`
  both branch on it to build much cheaper resources, which is the difference
  between 13 seconds and 129 for the hyperspace suite. It also covers the preview
  branch nothing else reaches.
- **Geometry built at setup is invisible in a frame.** Several savers compile
  everything into display lists in `initSaver` and only call them per frame, so
  settings that change geometry show up nowhere in a frame trace. That is what
  `startCapturingSetup()` in the fixture is for.
- **`GL_STUB_TRACE=1` echoes every GL call to stderr as it happens.** The recorded
  trace is no use when a saver crashes mid-frame, because the process dies before
  an assertion can read it. This is how Tasks 18, 19 and 21 were each located in
  about a minute.
- **`libs/Implicit` draws through vertex arrays**, not `glBegin`/`glEnd`, so its
  geometry lands in `arrayPrimitives` rather than `primitives` and must not
  disturb the begin/end pairing counts. A survey of `src/` alone misses this.

## Task 9 · C++20 — only after Task 3

Would unlock `std::format`, clearing SonarCloud `S6185`. Safe on the v145
toolset and GCC 11+, but Clang only became solid at 16+. A scan found no
`u8""` literals, `char8_t` or `consteval` usage to worry about.

C++23 is **not** baselineable yet — MSVC offers only `/std:c++23preview` on
v145, and GCC 13/14 plus Clang 17 are all partial.
