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

**Since the last revision, six savers gained tests (Task 17) and that work found
three real defects — Tasks 14, 15 and 16, of which 14 is now fixed.** Prefer
covering a saver before changing it; that is how all three turned up.

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
| 12 | Six savers carry private `rand()` copies | open |
| 13 | Clear-text `http://` URLs | open |
| 14 | `solarWinds` sets `readyToDraw = 1` on `WM_DESTROY` | **done** — PR #44 |
| **15** | **`fieldlines` nests `glBegin`, silently losing its line widths** | **new, open** |
| 16 | Destructors sized from live globals | **done** |
| **17** | **Test coverage rollout — 6 of 14 savers** | **new, in progress** |

Tasks 10–13 came out of the rslibs work and a re-read of SonarCloud. Tasks
14–16 came out of building the test harness (PRs #42, #43): all three are real
defects that thirteen years of running the savers never surfaced, and each is
now pinned by a test asserting current behaviour so the fix has a tripwire.

---

# Priority and order

**Ordering principle: things that can corrupt memory first, then the cheap
mechanical work that clears warning noise hiding them, then robustness against
bad input, and ratings-only refactors last.** The current quality gate cannot be
the guide here — see the note at the end of this section.

## First — correctness

1. **Task 16.** Destructors that size their frees from the current globals
   rather than from what they allocated. The same class of hazard as Task 14,
   which is **done**, but slightly more work — and the one that survives in
   `solarWinds` after that fix.
2. **Task 10, what remains.** The two `cyclone` BLOCKERs are **resolved** — see
   below — so what is left is `lattice.cpp:703` (uninitialised field) and the
   six `cpp:S836` findings in `skyrocket/particle.cpp`, all "garbage value
   returned to caller".
3. **Task 15.** `fieldlines` loses its per-segment line widths to nested
   `glBegin` calls. Visible only as a subtly wrong render, so lower than the
   memory bugs, but it is a genuine rendering defect rather than a lint.

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

### Still open in `src/`

| Where | Rule | Finding |
|---|---|---|
| `lattice.cpp:703` | `cpp:S2107` | Uninitialised field at the end of the constructor |
| `skyrocket/particle.cpp:68, 844, 874, 902, 938` | `cpp:S836` | Garbage value returned to caller (×5) |
| `lattice.cpp:275` | `cpp:S836` | Garbage value returned to caller |
| `cyclone.cpp:348` | `cpp:S2193` | MINOR |

The `skyrocket` cluster is the biggest single group left and none of it is
covered by tests yet — `skyrocket` is late in the Task 17 rollout precisely
because it is the hardest module. Consider pulling it forward if these are to
be fixed.

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

## Task 12 · Six savers carry private `rand()` copies

```bash
grep -rln "return rand() % x\|float(rand()) / float(RAND_MAX)" src --include=*.cpp
```

| Saver | Private copy |
|---|---|
| `cyclone`, `fieldlines`, `flocks`, `flux`, `plasma` | both `rsRandi` and `rsRandf` |
| `solarwinds` | `rsRandf` only |

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
  `starfield` alone that was 8 of 97 access sites. There is **no Linux build
  for `src/` in this repository at all** — no Makefile, no CMake — so those
  branches are never compiled by anyone, by CI or locally. Review them by eye;
  a green Windows build proves nothing about them.

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

## Task 17 · Test coverage rollout — IN PROGRESS

Before PR #42 nothing in `src/` was tested. Six savers now are.

| Saver | Coverage (CI) | |
|---|---:|---|
| `plasma` | 82.7% | PR #42 |
| `cyclone` | 80.6% | PR #42 |
| `flocks` | 70.1% | PR #42 |
| `fieldlines` | 76.2% | PR #43 |
| `solarWinds` | 79.0% | PR #43 |
| `starfield` | 75.2% | PR #43 |
| **Total** | **77.3%** | against a 60% target |

Coverage reads about five points higher locally, for the registry reason under
Task 11. A CI job posts a self-updating per-module table on every PR; it is
**reported, not gated**, because a threshold would fail every PR that adds a
saver before its tests.

**Remaining, in the intended order:** `flux`, `euphoria`, `helios`, then
`lattice`, `hyperspace`, `skyrocket`, then `microcosm`. `implicitDemo` is a
freeglut demo rather than a saver and needs a different shape.

Two of those need harness work first:

- `lattice`, `hyperspace` and `skyrocket` call `gluProject`, `glGetFloatv` and
  `glGetDoublev` and feed the results into real maths. The stub returns nothing
  meaningful, so they need either a minimal matrix stack in it or their
  projection maths extracted into pure functions. The second is the smaller
  change and was the original recommendation.
- `microcosm` starts two worker threads with four `while(1)` condvar loops
  (`microcosm.cpp:275-342`). A test that reaches the thread-start path hangs.

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

## Task 9 · C++20 — only after Task 3

Would unlock `std::format`, clearing SonarCloud `S6185`. Safe on the v145
toolset and GCC 11+, but Clang only became solid at 16+. A scan found no
`u8""` literals, `char8_t` or `consteval` usage to worry about.

C++23 is **not** baselineable yet — MSVC offers only `/std:c++23preview` on
v145, and GCC 13/14 plus Clang 17 are all partial.
