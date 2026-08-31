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
Tasks 18 to 23 from the last one. Eight are fixed. Prefer covering a saver before
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

**Every suite can be seeded now.** Seven savers used to carry private copies of
`rsRandi`/`rsRandf` — and `starfield` its own `rsRandGen` — so including
`<rsMath/rsMath.h>` in the common header put two definitions of the same inline
function in one binary and crashed `starfield` in Release while Debug stayed
green. That is what raised Task 12 from tidiness to undefined behaviour. The
private copies are gone, `saver_test_common.h` includes `<rsMath/rsMath.h>`,
and every suite seeds `rsRandGen()` to `kTestSeed` in its own fixture's
`SetUp`.

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
| 2 | Debug build warnings | **done** |
| 3 | Declare C++17 explicitly | **done** |
| 4 | `resource.h` include style | **done** — PR #37 |
| 5 | `aboutProc` truncates an `HBRUSH` | **done** — PR #39 |
| 6 | Encapsulate mutable module globals | open |
| 7 | `libs` submodule | **done** — rslibs L1–L5, L8; bumped in #40 |
| 8 | SonarCloud duplication | **done** — 1.6% project-wide, under the 3% threshold |
| 9 | C++20 | open, blocked on 3 |
| 10 | Reliability bugs | **partial** — the two cyclone BLOCKERs are proven unreachable; 8 bugs remain (`cpp:S6232` ×4, `cpp:S1763` ×2, `cpp:S836` ×2) per Task 10's own "Four findings — RESOLVED" section — the 10-bug count elsewhere in this document is an older SonarCloud snapshot, left for a fresh analysis rather than hand-derived |
| 11 | Registry values used unclamped | open |
| **12** | **Seven savers carry private PRNG copies — an ODR violation that crashes Release** | **done** |
| 13 | Clear-text `http://` URLs | open |
| 14 | `solarWinds` sets `readyToDraw = 1` on `WM_DESTROY` | **done** — PR #44 |
| 15 | `fieldlines` nests `glBegin`, silently losing its line widths | **done** |
| 16 | Destructors sized from live globals | **done** |
| 17 | Test coverage rollout — all 13 savers | **done** |
| **18** | **`hyperspace` drew through freed texture objects after teardown** | **new, done** |
| **19** | **`skyrocket` indexed an emptied particle vector after teardown** | **new, done** |
| **20** | **`helios` keeps two restart-unsafe statics, one an out-of-bounds read** | **new, done** |
| **21** | **`hyperspace` calls `glActiveTextureARB` with no extension check** | **new, done** |
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
   below. #53 fixed the `lattice` constructor field, the `makeTorus` guard and
   five of the `particle.cpp` garbage-value findings, and raised
   `particle.cpp` coverage from 17.6% to 95.3%. Task 10 stays **PARTIAL**:
   `cpp:S6232` ×4, `cpp:S1763` ×2 and `cpp:S836` ×2 remain — see the note
   under Task 10.
2. **Task 15 — DONE.** `fieldlines` no longer loses its per-segment line widths
   to nested `glBegin` calls; the `glEnd` guard now closes each per-segment
   strip before the next one opens. This changes rendered output — field lines
   now thin as they recede instead of drawing at one uniform width — while
   constant-width mode is unchanged.
3. **Task 25.** `skyrocket` will not start on a machine without OpenAL
   installed. Not a memory bug, but it is the only item on this list that makes a
   saver completely unusable for a real user, and shipping the redistributable is
   a packaging change rather than a code one.

## Second — cheap, mechanical, high value per minute

4. **Task 2 remainder — DONE.** Deleted `MinimalRebuild` from 13 projects and the
   Debug `LinkTimeCodeGeneration` from 9. This was a one-line-per-file
   substitution that removed **all 44 remaining Debug warnings** (26 `D9035`,
   18 `LNK4075`). Doing it early means every later task's build output is
   readable.
5. **Task 3 — DONE.** Added `<LanguageStandard>stdcpp17</LanguageStandard>` to
   both configurations of 13 projects. Trivial, and the submodule was already
   there, so this ended the split where the libs compiled as C++17 inside this
   very solution and the savers did not.
6. **Task 13.** 26 `http://` URLs across `.cpp` and `.rc`. A one-line change that
   clears 6 `cpp:S5332` findings, and these are live `ShellExecute` calls that
   open a browser, so it is a real if small user-facing fix.

## Third — robustness against untrusted input

7. **Task 11 — DONE.** All 112 saver-specific registry values, plus
   `dFrameRateLimit` in all 13 savers, are now clamped on read. This is the
   `src/` counterpart of the rslibs L5 work, and likely the root cause behind
   some of Task 10's findings, which is why it followed rather than led them.
8. **Task 12 — DONE.** The seven private rand()/rsRandGen copies are gone; all
   thirteen savers share rsMath.h's generator.

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

## Task 2 · Debug build warnings — DONE

`EditAndContinue` → `ProgramDatabase` landed in #39, clearing every
`LNK4075 '/EDITANDCONTINUE'`. The other two changes, both Debug configuration
only, landed here — Release was not touched:

| Change | Files | Cleared |
|---|---|---|
| Delete `<MinimalRebuild>true</MinimalRebuild>` | 13 | 26 × `D9035` |
| Delete `<LinkTimeCodeGeneration>` from the **Debug** `<Link>` group | 9 | 18 × `LNK4075 '/INCREMENTAL'` |

```bash
grep -rl "MinimalRebuild" src --include=*.vcxproj
```
Returns empty since Task 2 landed.

The 9 with Debug LTCG were: `cyclone flocks helios hyperspace lattice microcosm
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

Together these took a Debug rebuild from 44 warnings to **0**.

## Task 3 · Declare C++17 explicitly — DONE

**13 `.vcxproj`** — every one except `starfield`. Added to the `<ClCompile>`
group of **both** configurations:

```xml
<LanguageStandard>stdcpp17</LanguageStandard>
```

```bash
grep -rL "LanguageStandard" src/*/*.vcxproj
```
Returns empty since Task 3 landed.

**Why:** twelve savers plus `implicitDemo` declared no standard, so they landed
on the MSVC default (C++14) — `starfield` was the one saver that already
declared one — while `tests/` compiled as C++17 because GoogleTest forces it.
The submodule declares C++17 explicitly in all three of its build systems
(rslibs L3), and its projects are built *by this solution* — so one solution
used to compile its libraries as C++17 and its executables as C++14.

Set both configurations together. Debug and Release compiling different language
versions is a worse failure mode than the consistent implicit default they
had.

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
before `initSaver()`, and every path through `readRegistry` leaves `dComplexity`
bounded by `cycloneSettings::kComplexity` — `setDefaults` takes it from
`kDefaultComplexity`, and a stored value is clamped against the same range on
read. #35's separate unconditional `if(dComplexity < 1)/(> 10)` guard was
removed by #62, which made the header the single source of the bound.

Three `CycloneBlockerGuard` tests in `tests/test_cyclone.cpp` pin that reasoning,
including the ordering. **They still report as BLOCKERs in SonarCloud** — the
analyser cannot see the clamp across functions — so either mark them *Safe*
there or leave them with this note attached. Do not "fix" them by adding casts.

> One caveat, written up in full in the test file: `readRegistry` returns early
> when the saver has never stored settings, which is the case on a fresh CI
> runner. There the clamp lines never execute and the guard only confirms that
> `setDefaults`' value survives — which is now the whole guarantee on that path,
> since `setDefaults` reads `kDefaultComplexity` from the same header that
> bounds the registry read. Task 11 is done and did **not** make the clamp
> itself unconditional: the registry read still only runs where a key exists.
> What Task 11 did do is make the clamp a pure function, testable without a
> registry at all, and add a source-text check (`SettingsClampWiring`) that
> catches the clamp being pointed at the wrong range constant — which a runtime
> test cannot, because a wrong-but-similarly-shaped range still passes a range
> check.

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

## Task 11 · Registry values are used unclamped — DONE

**13 savers, 112 saver-specific assignments plus 13 `dFrameRateLimit` reads, all
now clamped, and all 125 reads now type-checked.**
`src/common/saverSettings.h` holds the two pure clamp primitives
(`rssaver::clampToRange` for a raw `DWORD`, `rssaver::clampIntToRange` for a
value already held in a signed `int`); each saver's `<name>Settings.h`
re-exports them and declares its own `Range` per setting; `readRegistry` routes
every `d* = val;` assignment through the matching clamp, and every
`dFrameRateLimit` read goes through a clamp too — `rsWin32Saver::clampFrameRateLimit`
in twelve savers (including `starfield`, which used to assign it raw), and
`cycloneSettings::clampFrameRate` in `cyclone`, which keeps the dialog's
"0 means unlimited" semantics.

**The type guard is closed too.** `src/common/saverRegistry.h` holds
`rssaver::readRegistryDWORD`, which reports success only when the value exists
*and* is a `REG_DWORD` of the expected size. Every one of the 125 reads goes
through it, so a `"Speed"` stored as `REG_SZ` now leaves `setDefaults`' value in
place instead of arriving as the first four bytes of its text reinterpreted as a
`DWORD`. Earlier revisions of this task listed that as out of scope; it was
closed in the same pass, generalising the helper `cyclone` introduced in #62.
`saverRegistry.h` is a separate header from `saverSettings.h` on purpose — the
latter must stay free of `windows.h` so the ranges and clamps stay testable
without an `HWND`, which `tests/test_saverSettings.cpp` compiling at all is what
asserts.

**What this does not do — read before relying on it.** The design is
per-assignment clamping with `readRegistry`'s early return left in place: on a
fresh CI runner, or any machine where a saver has never stored settings,
`readRegistry` returns before any registry read runs, so **no clamp line
executes at all** on that path. The postcondition (every setting ends up inside
its declared range) still holds there, but only because `setDefaults` already
picks values inside range — not because of anything this task added.

**The pairing gate.** A mis-paired range constant — `dSize` clamped against
`kSpeed`, say — compiles clean and, because it sits below the early return,
never executes on CI regardless of what the ranges contain. No runtime
assertion can be relied on to catch that reliably (see the cyclone note below
for why a range check specifically cannot). `tests/tools/check-settings-wiring.cmake`,
registered as the `SettingsClampWiring` ctest case, is a source-text check
instead: it parses each saver's `readRegistry` and asserts the clamped name and
the range constant name agree, that the per-saver and total counts match a
checked-in table (112), that no raw `dX = val;` survives, that all 13
`dFrameRateLimit` sites are wired, that every read is typed (no bare
`RegQueryValueEx`, and the per-saver read counts match), and that nothing
redefines `readRegistryDWORD` privately. It runs on every `ctest` invocation, so
a future mis-pairing fails the build rather than compiling clean.

Former detection grep, now the standing regression tripwire for this task —
expected output **0**, not ~124:

```bash
grep -rn "^\s*d[A-Za-z]* = val;" src/*/*.cpp | wc -l
```

`SettingsClampWiring` enforces the same rule automatically (rule 5 in the
script), so nobody has to remember to run the grep by hand.

**cyclone is the one saver that does not follow the shared shape**, because it
arrived at the same place independently in #62 and its settings header also
drives the redesigned dialog. Three differences, all deliberate:

- Its two checkboxes go through `cycloneSettings::normalizeFlag` rather than a
  `{0, 1}` `Range`, so it contributes 5 rows to the 112 rather than 7.
  `tests/test_cyclone.cpp` asserts them as flags instead.
- `dFrameRateLimit` goes through `cycloneSettings::clampFrameRate`, which keeps
  the dialog's "stored 0 means unlimited" contract that
  `rsWin32Saver::clampFrameRateLimit` has no notion of.
- It declares its own `Range` rather than re-exporting `rssaver::Range`. The two
  are layout-identical; `tests/test_saverSettings.cpp` widens its rows
  explicitly rather than churn a header that had just landed.

`SettingsClampWiring` encodes each of these as cyclone's expected shape rather
than waiving them, so cyclone is as tightly checked as the other twelve.

`cyclone.cpp` no longer carries the unconditional
`if(dComplexity < 1)/(> 10)` guard that #35 added and that earlier revisions of
this task described keeping as `clampIntToRange`: #62 removed it, because
`setDefaults` now takes `dComplexity` from `kDefaultComplexity` and the registry
read clamps against the same `kComplexity`, so every path into `initSaver` is
bounded by the header. The `CycloneBlockerGuard` tests still pin that
end-to-end. `rssaver::clampIntToRange` remains in the shared header for the
signed-input case — `clampToRange((unsigned long)-5, {1,10})` returns **10**,
the upper bound, not `1`, so the two are not interchangeable — and
`SaverSettings.ClampIntToRangeSendsNegativesToTheLowBound` keeps it honest.

**Two things the test work established.**

Every saver now has a postcondition test named for what it proves rather than
how — `<Saver>Framework.ReadRegistryLeavesEverySettingInsideItsDeclaredRange` —
because on the no-key path the clamp never runs and `setDefaults` is what keeps
the value in range there instead. `starfield`'s equivalent test
(`StarfieldFramework.ReadRegistryLeavesEverySettingInsideItsDeclaredRange`,
renamed from `...ReadRegistryClampsEveryValueIntoRange`) is no longer a special
case: the postcondition holds on both paths through `readRegistry`, by the
clamp on one and by `setDefaults` on the other, exactly like every other saver.
`starfield.cpp:332-338` sits below its own early return at `:325-326` the same
as everywhere else.

That is also why this task could not, by itself, fix coverage elsewhere. Every
saver's `readRegistry` still returns early when the key is absent — the case on
a fresh CI runner — so on CI those clamp lines still never run. Coverage is
still about five points lower on CI than on a developer machine that has run
the savers, and the `cyclone` BLOCKER guard (Task 10) still only bites fully
where a key exists — the pure-function refactor made the clamp *testable*
without a registry (`tests/test_saverSettings.cpp`, no `HWND` and no
`RegQueryValueEx` involved), which is a real improvement, but it did not touch
`readRegistry`'s control flow, so the coverage gap this section used to predict
would close is still there. Restructuring to also clamp on the no-key path was
considered and rejected — see the design-fork discussion in the Task 11 plan
history — because a second, hand-written pass over 112 settings is itself a
live mis-pairing risk, for a runtime property the wiring check already proves
statically.

## Task 12 · Seven savers carry private PRNG copies — and it is an ODR violation — DONE

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
| **`starfield`** | **`rsRandf` and its own `rsRandGen`** (`starfield.cpp:80`) |

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

Both guards were comparisons that a NaN slipped through. The bucket index is
now bracketed into `[1, maxStarSize]` by an is-in-range test, so a bad value
cannot reach `sizeBuckets[]` — pinned by
`Starfield.NanFrameTimeKeepsStarsInsideTheSizeBuckets`. Unification narrowed
how a NaN can arrive at all (`rsMath.h`'s `rsRandf` scales a canonical `[0, 1)`
value rather than calling `uniform_real_distribution` with a possibly-negative
`x`), but it does not make NaN-versus-comparison a solved problem everywhere
else in this saver or the others.

The private copies are gone: `tests/support/saver_test_common.h` includes
`<rsMath/rsMath.h>`, and all thirteen suites seed `kTestSeed`.

Each is a verbatim duplicate of what `rsMath.h` used to contain, carrying the
same *"Don't forget to initialize with srand()"* comment. **rslibs L4 did not
reach them**, because they never include `rsMath.h` — contrary to what the
rslibs brief predicted.

The copies were deleted and each saver now includes `<rsMath/rsMath.h>`
instead. That inherits the thread-local Mersenne Twister, removes the modulo
bias, and deleted six blocks of duplicated code — a down payment on Task 8.

The `srand((unsigned)time(NULL))` calls this made dead in six savers were
removed along with the copies. Six more dead calls survived in modules that
already used the library generator (`euphoria`, `helios`, `hyperspace`,
`lattice`, `microcosm` and `skyrocket`); those are now removed too. The
`NoLegacyCPrngCalls` CTest gate scans parent-project sources so a direct
`rand()` or `srand()` call cannot silently reintroduce a second generator.

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

Three defects that thirteen years of running these savers never surfaced. All
three — Task 14, Task 15 and Task 16 — are now fixed. Each was pinned by a test
asserting the **prior** behaviour, so fixing it made its own test fail — that
was the point. The test said so in its own comment.

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

## Task 15 · `fieldlines` nests `glBegin` and loses its line widths — DONE

With `dConstwidth` false — the **default** — `drawfieldline` reopened a
`GL_LINE_STRIP` on every step so it can vary `glLineWidth` per segment
(`fieldlines.cpp:237-240`), but the matching `glEnd` only ran on the final step
(`fieldlines.cpp:249-250`). Every intermediate `glBegin` therefore landed inside
an already-open block.

A driver treats each as `GL_INVALID_OPERATION` and ignores it — along with the
`glLineWidth` calls between them. So **the per-segment width the code is
reaching for never applied**, and the line rendered as one uniform strip. It
looked plausible, which is why it survived; it was also spamming GL errors
every frame.

**Fixed:** the `glEnd` guard is now `if(!dConstwidth || i == (int(dMaxSteps) -
1))`, so each per-segment strip closes before the next one opens. The
identically-worded guard one line above (`fieldlines.cpp:244`, the black
end-cap `glColor3f(0.0f, 0.0f, 0.0f)`) is deliberately left unchanged.
Constant-width mode is byte-for-byte unchanged — the new disjunct is always
false there — and the early-termination path into an ion still works the same
way in both modes: the collision iteration skips the new `glEnd` along with the
rest of its guarded block, so its strip stays open for the post-loop code to
finish into the ion and close. The pinned test
`Fieldlines.DefaultModeLeavesGlBeginBlocksUnclosed` was deleted and replaced by
`Fieldlines.PairsBeginAndEndInBothWidthModes` and
`Fieldlines.DefaultModeSetsLineWidthOncePerStrip`. This changes what the saver
looks like: field lines now thin as they recede instead of drawing at one
uniform width.

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

## Task 26 · `microcosm` appends its gizmo list instead of rebuilding it — DONE

`initSaver` pushed 55 gizmos onto `gizmos`, and the `clear()` that should have
come first was inside the comment on the line above them:

```cpp
	// initialize gizmos	gizmos.clear();      // was microcosm.cpp:979
	{ Metaballs* gizmo = new Metaballs(7);  gizmos.push_back(gizmo); }
```

A tab, not a newline, between the comment text and the statement, so it never
ran: a second `initSaver` left 110 entries and `chooseGizmo` picked at random
from the doubled range. `cleanUp` freed nothing at all, so the 55 original
`Gizmo` objects leaked as well.

The last entry is an easter-egg gizmo that `chooseGizmo` withholds unless
`gTennisAvailable`, by dropping one off the top of its random range. With the
list doubled that guard still excluded only the *final* entry — so the first
copy's easter egg became reachable at random. That was the one visible
consequence beyond the leak.

**Fixed.** Splitting the line was the one-character half. Freeing the gizmos
turned out to be the larger half for a reason the brief did not record: the
ownership model underneath could not support a `delete` at all.

1. **`Gizmo::~Gizmo` was not virtual** (`gizmo.h:63`), so deleting through the
   `Gizmo*` the saver holds would have run no subclass destructor — undefined
   behaviour, and every shape leaked anyway. It is now `virtual`, and it is the
   single owner of `mShapes`: it deletes each shape rather than just clearing
   the vector.
2. **18 subclasses each carried an identical `~X(){ for(...) delete mShapes[i]; }`**
   which, once the base freed them, would have been a double free. All 18 are
   gone. So is `~Orbit`, whose `torus1..3` are aliases into `mShapes`; `TorusBox`
   holds the same aliases and never had one.
3. **`Brain` keeps its three `delete[]`** and **`RingOfTori` gained the one it
   never had** — those are arrays *of* pointers, held alongside `mShapes` rather
   than inside it, and they were leaking independently of any restart.
4. **`cleanUp` deletes the list, clears it, and resets what indexes it**:
   `gGizmoIndex`, `shapes` (borrowed pointers into the gizmos just freed),
   `gNumShapes`, and `readyToDraw` — `screenSaverProc` clears that last one
   before calling `cleanUp` (`microcosm.cpp:1539`), but `cleanUp` is reachable
   directly and `draw()` dereferences `gizmos[gGizmoIndex]` unconditionally. The
   leak was what made that safe before.
5. **`easterEggTime` was hoisted** out of `draw()` to file scope as
   `gEasterEggTime` and is reset with `gTennisAvailable`. Resetting the flag
   alone would have been theatre: the static outlives it and unlocks the egg
   again on the next frame.
6. **`chooseSpecificGizmo`'s `gizmos.size() - 1`** (`microcosm.cpp:1458`) is
   unsigned and now guarded by a size check. It could not underflow while the
   list was never emptied; it can now.

`crawlpoints` needed nothing: `impCrawlPoint` is three floats by value, and
`draw` clears the vector every frame anyway.

Pinned by three cases in `tests/test_microcosm.cpp`:
`GizmoListRebuiltOnEveryRestart` (the size is now the same on every start, and
the list is empty between them), `EasterEggStaysHiddenAcrossRestarts` (the
visible symptom, asserted directly), and `GizmoDestructionIsVirtual` — a
`static_assert` on `std::has_virtual_destructor<Gizmo>` plus a delete through
`Gizmo*` of one gizmo per ownership shape, wrapped in a `_CrtMemDifference`
check. The leak check was verified against a deliberate leak before being
committed; without that it would assert nothing.

### What this does not do

`cleanUp` still leaks the three volumes, the six surfaces, `tex1d`, `textwriter`
and the two thread handles, and `draw()`'s remaining statics (`first`,
`transitionTime`, the camera vectors) still survive a restart. Both are the
Task 27 pattern rather than this one, and neither is reachable from the shipped
saver, which exits on `WM_DESTROY` rather than restarting.

`impShape` (`libs/Implicit/impShape.h:56`) has virtual functions and a
non-virtual destructor, which is the same defect one level down — every
`delete mShapes[i]` formally relies on it. No shape subclass owns memory, so
nothing leaks today. It belongs to rslibs; recorded in
`docs/MAINTENANCE-rslibs.md` rather than fixed from here.

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

## Task 21 · `hyperspace` calls `glActiveTextureARB` unguarded — DONE

`initSaver` degrades properly when the extensions are missing:

```cpp
if(!initExtensions())
    dShaders = 0;          // hyperspace.cpp:532-533
```

and every use of the ARB entry points sat inside `if(dShaders)` — except three:

```cpp
glActiveTextureARB(GL_TEXTURE2_ARB);   // were hyperspace.cpp:238
glActiveTextureARB(GL_TEXTURE1_ARB);   // 240
glActiveTextureARB(GL_TEXTURE0_ARB);   // 242
```

Those are function pointers that `initExtensions` only fills in on success, so
without `GL_ARB_multitexture` the first frame called through address zero. The
"graceful fallback" was a crash.

Unreachable in practice — no GPU since roughly 2002 lacks the three extensions
the loader asks for — which is why it had never been reported.

**Fixed:** the three calls now sit inside the same `if(dShaders)` the rest of
the star block already uses (`hyperspace.cpp:245, 247, 249`). The trailing
`glBindTexture(GL_TEXTURE_2D, flaretex[0])` stays outside the guard: with it
in place, no `glActiveTextureARB` call anywhere in the binary runs when
`dShaders` is 0 — the only other call sites are the goo's three-unit cube map
and the tunnel's second caustic frame (both already inside `if(dShaders)`,
`hyperspace.cpp:310, 312, 314` and `365, 367`) and `starBurst.cpp:225, 227,
229`, reached only from `starBurst::draw(float)`, which `hyperspace.cpp`
calls only under `if(dShaders)` — so GL's default texture unit 0 is never
left, and the unconditional bind lands where it should. The `dShaders == 1`
call sequence is unchanged; `Hyperspace.ResetsTheOtherTextureUnitsWhenShadersAreOn`
pins it at exactly three calls per frame.

**Consequence for the harness:** the GL stub still *advertises*
`GL_ARB_multitexture`, `GL_ARB_texture_cube_map` and `GL_ARB_shader_objects` and
resolves their entry points by default — that is the path real hardware takes
— but `glstub::setExtensionString` now lets a case report fewer, which is how
`Hyperspace.DropsShadersWhenTheArbExtensionsAreMissing` drives hyperspace's
loader-failure path end to end. `microcosm` keeps its existing coverage of the
non-shader *draw* path through `Microcosm.RendersWithoutShadersToo`, which sets
`dShaders` directly; its `initExtensions`-failure path is now reachable through
the same toggle but remains uncovered, left for whoever wants it, since
microcosm has a working fallback and no null call to make.

What the newly reachable path exposed: `initSaver`'s non-shader branch
(`hyperspace.cpp:617-643`) rewrites the global `nebulamap` array in place, so a
second no-shader `initSaver` in the same process squares the darkening — a
sixth sighting of the restart-safety theme running through this document.
Cosmetic, unreachable outside a test fixture on a pre-2002 GPU, so recorded
here rather than opened as its own task.

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

## Task 6 · Encapsulate mutable module globals (SonarCloud `cpp:S5421`) — IN PROGRESS

**721 findings, every one at CRITICAL/HIGH impact.** The rule emits two messages —
`Global variables should be const.` and `Global pointers should be const at every level.`
— and they are the same defect with the same remedy. Measured on `src/hyperspace`, the
split is 73 to 30.

The pointer half is not separable. Of the 117 pointer-typed globals in `src/`, 53 are
objects `new`'d in `initSaver` and `delete`d in `cleanUp`, 21 are GL extension entry
points assigned in `initExtensions()`, and 30 are `HDC`/`HGLRC`/`HANDLE` values created at
window and thread setup. None can be `const` at any level. They clear only by leaving
namespace scope.

Baseline by Sonar component, before this task started:

| Component | Count | Component | Count |
|---|---|---|---|
| `src/skyrocket` | 116 | `src/fieldlines` | 18 |
| `src/hyperspace` | 103 | `src/plasma` | 18 |
| `src/microcosm` | 98 | `src/cyclone` | 17 |
| `src/lattice` | 37 | `src/solarwinds` | 16 |
| `src/euphoria` | 32 | `src/starfield` | 14 |
| `src/flux` | 30 | `src/implicitDemo` | 10 |
| `src/helios` | 27 | `tests/test_*.cpp` | 151 |
| `src/flocks` | 23 | `tests/support/saver_shim.cpp` | 11 |

`src/common` contributes zero — both shared headers are pure `constexpr`/`inline`.

### The pattern

One header per saver, `src/<name>/<name>State.h`, holding a `State` struct reached through
a function-local static defined in the saver's main `.cpp`. `src/starfield/starfieldState.h`
is the worked example.

```cpp
namespace starfieldState {
struct State {
    State() = default;
    State(const State&) = delete;      // see the copy trap below
    State& operator=(const State&) = delete;
    int readyToDraw = 0;
    /* ...module-specific state... */
};
State& state();                       // { static State s; return s; }
}
```

The accessor needs **external** linkage: the test suite and, in a multi-TU saver, the
sibling translation units both call it. For those savers the state header **replaces** the
cross-TU `extern` blocks outright (`src/skyrocket/particle.h`, `world.h`, `flare.h`,
`smoke.h`; `src/microcosm/mirrorBox.cpp`; `src/hyperspace/starBurst.cpp` and four
siblings). Include direction is one way: the state header includes the type headers, never
the reverse.

**This is why the pattern works at all.** `tests/support/gl_stub.cpp:34-42` and `:336`
already hold all the stub's state behind `Trace& trace(){ static Trace instance; return
instance; }`, and `tests/support/` is charged **zero** findings apart from the shim's
eleven. Note that `static` alone does not help — `src/skyrocket/skyrocket.cpp:70-71` is
already `static` and skyrocket is still charged 116. Internal linkage is not what the rule
measures.

### Rules for every migration

- **Pure move, no semantic change.** No `unique_ptr` conversions, no added null-outs, no
  wholesale `state() = State{}` in `cleanUp`. `hyperspace.cpp:104-115` documents that its
  two deliberate nulls *are* the "already built" guard the draw path reads, and `helios`'s
  missing nulls stay missing. A wholesale reset would also zero the settings, which now
  live in the same struct.
- **Leave `draw()`'s function-local statics alone.** They are invisible to Sonar, and
  moving them changes restart behaviour.
- **`auto& s = state();`, hoisted once per function, never inside a loop.** The
  thread-safe-static guard is one atomic load per call.
- **It is `auto&`, never `auto`.** The deleted copy constructor turns a dropped ampersand
  into a compile error rather than a silent copy whose writes go nowhere. For `plasma` that
  copy would be about 32 MB.
- **Guard the statistics overlay's text writer, in that saver's own migration PR.** Where
  `textwriter` is created in `initSaver`, write `if(kStatistics && s.textwriter)`. The struct's
  explicit `= nullptr` makes a null provable that the old uninitialised global hid, and
  SonarCloud raises `cpp:S2259` (a **bug**, so it fails the PR gate on
  `new_reliability_rating`) wherever its path exploration happens to reach it. It reached
  cyclone but not the byte-identical solarwinds, so it cannot be predicted per saver. Eager
  construction is not an alternative — `rsText::rsText()` calls `glGenTextures` and
  `glGenLists` immediately (`libs/rsText/rsText.cpp:25-37`), while the struct is built on the
  first `state()` call, in `readRegistry` at `WM_CREATE`, before any GL context exists.

  Two things not to do. Do **not** add the guard to `flocks`, `microcosm` or `skyrocket`:
  those create `textwriter` at the top of `draw()` itself, so the pointer is assigned before
  the dereference in the same function and the condition would be dead. And do **not** add it
  to already-migrated savers speculatively — see the duplication note below. `starfield`,
  `plasma` and `solarwinds` carry no `cpp:S2259` today; if one ever appears, guard that saver
  alone.

- **Never edit the same duplicated boilerplate in several savers in one PR.** The FPS/overlay
  block, `readRegistry`/`writeRegistry` and the dialog procedure are duplicated across all
  thirteen modules (Task 8). Touching one line of the overlay block in four savers at once
  put new lines inside four copies of a known-duplicated region and drove
  `new_duplicated_lines_density` from 1.0% to **3.2%**, past the 3% gate — while the state
  headers themselves measured zero duplication. Inside a single migration PR the same edit is
  harmless, because that saver's hundreds of new lines dominate the denominator. One saver per
  PR, as the rest of this task already requires.
- **`const auto& s = state();` wherever the function only reads state** (`cpp:S5350`). The
  compiler is the check. `const State&` does not propagate through the pointer members, so
  `s.cyclones[i]->update()` still compiles.
- **Spell the include `<Windows.h>`** in state headers (`cpp:S3806`). That is the SDK's own
  filename and what every file under `tests/` already uses. The savers' own `.cpp` files use
  the lowercase spelling; leave those alone, they are pre-existing and out of scope.
- **Watch the failure-message strings when renaming mechanically.** A search-and-replace
  over a suite rewrites moved identifiers inside `<< "..."` prose too. `test_plasma.cpp`
  had three such messages; `test_starfield.cpp` happened to have none. Check with
  `grep -n '"[^"]*state()\.'` before building — the result compiles either way.
- **Watch for shadowing.** `starfield`'s render loop used `for(int s = 1; ...)`, which hid
  the hoisted reference; it was renamed to `bucketSize`. The projects build at
  `/W3` and MSVC's shadowing warnings (C4456/C4457) are `/W4`, so nothing warns — these
  are caught only because member access on the shadowing object fails to compile. A sweep
  of the remaining modules found exactly two more collisions, both of which will be
  compile errors when their migration lands: `fieldlines.cpp:267` (`static float s`) and
  `microcosm`'s five `setScale(float s)` bodies (`gizmo.cpp:60`, `knotAndSpheres.h:52`,
  `metaballs.h:47`, `spheresAndCapsules.h:54`, `triangleOfSpheres.h:44`). Rename the local,
  not the hoisted reference.
- **Keep the `d` prefix, one-line clamps and the local `HKEY skey`.** `SettingsClampWiring`
  reads the source text.
- **Review the `#ifdef RS_XSCREENSAVER` blocks by eye.** Nothing compiles them.

### Two traps

- **Multi-TU modules.** `skyrocket`, `hyperspace` and `microcosm` genuinely share
  `frameTime` and `aspectRatio` across translation units, so they must be sliced by
  variable group rather than by file — a variable has to move in every TU at once.
- **Relocated dynamic initialisers shift the PRNG stream.** `microcosm.cpp:114`'s
  `MirrorBox` constructor (`mirrorBox.cpp:47-49`) draws 12 values during static init, that
  is *before* a test fixture seeds `rsRandGen()`. As a struct member it would draw after.
  That changes production visuals too, not only a test baseline, so it has to be a stated
  decision rather than a side effect. `rsCamera` (`microcosm.cpp:88`) touches no PRNG and
  is safe.

### Progress

- **Step 1 — free wins, done.** `const` on the 34 read-only texture and sound blobs, the
  dead `simulationTime` declaration in `goo.cpp`, and four framework `extern`s in the test
  suites replaced by an `rsWin32Saver.h` include. About 39 findings.
- **Step 2 — `starfield` pilot, done.** `src/starfield/starfieldState.h`, and the suite
  reaches the saver through `starfieldState::state()` instead of declaring seven `extern`s.
  14 findings in `src/` and 7 in the suite. `starfield.cpp` is down to one namespace-scope
  mutable, `registryPath`.
- **Step 3 — `plasma`, done.** 18 findings in `src/` and 10 in the suite, and the first
  saver whose `#define`d dimensions (`TEXSIZE`, `NUMCONSTS`) had to become `constexpr` in
  the state header so the struct could declare its arrays. `plasma.cpp` is down to
  `registryPath`.
- **Step 4 — `solarwinds`, done.** 16 findings in `src/` and 10 in the suite. Its `wind`
  class is forward-declared in the state header, since the struct only holds a pointer to
  it, and `wind`'s own `c`/`ct`/`cv` members are not globals despite matching plasma's
  names — build each saver's moved-name list from its own globals, never from the last
  saver's. `float lumdiff;` was deleted rather than moved: it is dead here, and live only
  in `flux`, which is evidently where it was copied from.
- **Step 5 — `cyclone`, done.** 17 findings in `src/` and 9 in the suite. It is the only
  saver with no `RS_XSCREENSAVER` code at all, and the only one whose `#define`s
  (`wide`, `high`) are unqualified object-like macros — the state header has to be included
  above them, or any header pulled in later that uses either word is rewritten. Its
  `CycloneBlockerGuard` cases, which pin the invariant behind the two `cpp:S3519` blockers,
  now set `state().dComplexity` and pass unchanged.
- **Step 6 — `fieldlines`, done.** 18 findings in `src/` and 8 in the suite. It is the first
  saver to hit the shadowing collision this task predicted: `draw()` held
  `static float s`, the half-step distance passed to the eight `drawfieldline` calls. Renamed
  to `axisStep` **before** hoisting, so the redefinition never happened. `microcosm`'s five
  `setScale(float s)` bodies are the one remaining collision on the list.
- **Remaining:** the other eight savers and `implicitDemo`, then the `registryPath` change
  described below.

### What a migration PR looks like on SonarCloud

A migration rewrites most lines of its saver, so **that saver's pre-existing smells are
attributed to the PR as new code**. Expect 10-40 findings per saver; the four merged so far
reported 1, 8, 8 and 24. Only a *bug*-type finding can fail the gate — the maintainability
rating stayed at 1 in every case. Do not treat the count as damage done by the migration, and
do not fix pre-existing smells inside a migration PR: the standing dispositions are

- **`cpp:S5025` raw `new`/`delete`** — the ownership refactor this task defers by rule. In
  cyclone it is also the exact code the two `cpp:S3519` blocker findings sit in.
- **`cpp:S5955` loop-variable declarations** — pre-existing C89 style in the same code.
- **`cpp:S5945` C-style arrays in the state headers** — worth doing, but separately: plasma's
  six are passed straight to `glTexSubImage2D`/`glTexImage2D` and would need `.data()` at each
  call site.

### The gate

`tests/tools/check-module-globals.cmake`, registered as the `ModuleGlobalsEncapsulated`
ctest case, pins each saver's namespace-scope mutable count and fails on any unqualified
use of a migrated name. Lower a number as each migration lands; never raise one. It is the
only check that can catch a missed reference inside an `RS_XSCREENSAVER` block, and the
only offline signal at all — this project uses SonarCloud Automatic Analysis, with no
`sonar-project.properties` and no scanner step in CI, so nothing here reproduces locally
and a delta is confirmed only after `main` is reanalysed.

The count heuristic does not need a C++ parser, because these files are uniformly
formatted: globals sit at column 0 and end in `;`, function definitions at column 0 end in
`{`, and prototypes end in `);` with no `=`. Verified against `plasma.cpp` and
`lattice.cpp`, where it selects exactly their global blocks.

**A defect worth knowing about while writing any of these gates:** CMake lists drop empty
elements, so iterating `file(STRINGS)` output loses every blank line and each reported line
number comes out short by the number of blanks above it. In `starfield.cpp` that is 59
lines by the middle of the file. `check-module-globals.cmake` works around it with
`read_numbered_lines`; **`check-settings-wiring.cmake` still has the bug** and mis-reports
the line in every one of its failure messages.

### What cannot be fixed here

- **`tests/support/saver_shim.cpp:60-72` — 11 findings, permanent.** `rsWin32Saver.h`
  declares them `extern` and the savers write them, so the shim must define them at
  namespace scope with those names or nothing links. The file argues this at `:51-59`.
  Hiding a declaration in block scope does not help: an accessor wrapping the extern was
  tried, measured, reported the identical count, and reverted. Do not repeat it.
- **`LPCTSTR registryPath` × 13 — needs a coordinated rslibs change.** This document
  previously called it "already exempt as pointer-to-const"; that was wrong. `LPCTSTR` is
  `const char*` and the rule wants `const char* const`, so all 13 are in the 721. Plain
  `const` at namespace scope gives internal linkage and the lib's `extern` goes unresolved,
  while a mismatched pair is MSVC C2373. It needs `extern LPCTSTR const registryPath;` in
  rslibs and 13 matching definitions here, landing together with a submodule bump. Twelve
  of the 13 also use a bare narrow literal rather than `TEXT(...)`; only `starfield.cpp` is
  correct.

**After the full rollout the residual is 11, not the 79 this document used to predict.**
That older figure assumed the test suites keep their `extern` declarations. Routing them
through `state()` costs nothing extra — the header has to exist for the multi-TU savers
anyway — and it clears all 151.

**Priority note:** this affects only the maintainability rating, which is the one rating
still at **A**. It clears no failing gate condition.

One thing to keep true: **`src/` should gain no new ones.** PR #46 briefly added a single
global to `hyperspace` and it was removed again in `7e36902` by keying off pointers that
already carried the state. `ModuleGlobalsEncapsulated` now enforces that automatically.

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
remains worth doing** on its own merits (it would largely resolve Task 6; Task
12, which it would also have absorbed, is closed on its own), it just no
longer has a failing gate behind it.

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
