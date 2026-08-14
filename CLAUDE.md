# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Terence M. Welsh's Really Slick Screensavers, imported from SourceForge SVN and kept building on
current Visual Studio. Thirteen OpenGL screensavers under `src/`, each a self-contained Win32
executable, plus `implicitDemo` (a freeglut demo, not a saver). Supporting libraries live in the
`libs` submodule ([rslibs](https://github.com/reallyslickscreensavers/rslibs)).

`docs/MAINTENANCE.md` is the working backlog — 26 numbered tasks with priorities, evidence and
`grep` commands. **Read it before changing anything in `src/`**; most non-obvious constraints
below are recorded there in full. `docs/MAINTENANCE-rslibs.md` is the same for the submodule.

## Building

```bash
git submodule update --init --recursive
msbuild src\rssavers.sln /p:Configuration=Release /p:Platform=x86 /t:Rebuild
```

- **Build the solution, never a single `.vcxproj`.** The solution is what builds the five library
  projects it references via `..\libs\...`; alone, a saver project finds no `rsWin32Saverd.lib`.
- **Use `/t:Rebuild`, not `Build`.** Warnings such as `D9035` only appear when source is actually
  recompiled, so a plain `Build` looks clean when it is not.
- **Close anything running from `bin\` first**, or the link fails with `LNK1104`.
- x86 only. Output lands in `bin\`, and a post-build event copies each `.exe` to `.scr`.

`installer/installer.iss` needs Inno Setup; `appveyor.yml` builds it under the `Installer`
configuration on tags. GitHub Actions (`.github/workflows/ci.yml`) builds Debug and Release and
runs the tests — Release plain, Debug under coverage.

The `Makefile` in each saver directory is dead: it targets an `RS_XSCREENSAVER` Linux build that
this repository has no working configuration for. `#ifdef RS_XSCREENSAVER` blocks are compiled by
nobody, so a green Windows build proves nothing about them.

## Tests

```bash
cmake -S tests -B tests/build -A Win32
cmake --build tests/build --config Debug
ctest --test-dir tests/build -C Debug --output-on-failure
```

One test executable per saver plus `test_gl_stub` and the settings-only `rssavers_tests`. Single
test or suite:

```bash
ctest --test-dir tests/build -C Debug -R Hyperspace --output-on-failure
tests/build/Debug/test_hyperspace.exe --gtest_filter=Hyperspace.RestartingRebuildsTheGeneratedTextures
```

Coverage, which CI reports on every PR — Debug only (Release inlining ruins line attribution and
OpenCppCoverage needs Debug PDBs):

```bash
OpenCppCoverage.exe --sources %CD%\src --excluded_sources %CD%\tests --cover_children \
  --export_type cobertura:coverage.xml -- ctest --test-dir tests/build -C Debug -j 8
pwsh tests/tools/coverage-report.ps1 -ReportPath coverage.xml
```

`--sources` must be **absolute** — the MSVC CRT's own sources sit under paths containing `\src\`
and a bare filter drags the whole runtime into the denominator. Do not add `--modules` (measured
38× slower on one suite) and do not pass `--timeout` (instrumented suites exceed short limits).

## Architecture

### Saver framework

`libs/rsWin32Saver` owns `WinMain`, the GL context, the message loop and the frame-rate limiter.
It parses the standard screensaver command line (`/c`, `/p <hwnd>`, `/s`, `/w`; `-` accepted for
`/`, only the first letter examined) in `rsWin32SaverSettings.h` and dispatches through a
`SaverOps` table. Each saver executable supplies the other half:

| Symbol | Role |
|---|---|
| `LPCTSTR registryPath` | `Software\Really Slick\<Name>`, the only global the framework mandates |
| `initSaver(HWND)` | allocate everything |
| `draw()` | one frame |
| `idleProc()` | ticks `rsTimer`, sets `frameTime`, calls `draw()` |
| `cleanUp(HWND)` | free everything |
| `LONG screenSaverProc(HWND, UINT, WPARAM, LPARAM)` | `WM_CREATE` → `readRegistry()` + `initSaver`, `WM_DESTROY` → `cleanUp` |
| `setDefaults()` | `(int which)` in the six savers with presets, `()` in the other seven |

**Three savers deviate** (Task 24): `helios` has `doSaver(HWND)` and no `initSaver`; `skyrocket`
spells teardown `cleanup` (lowercase u); `flux` returns `LRESULT`. Each is a link error, worked
around by a one-line shim in the test file rather than by touching saver source.

Settings are module globals named `d*` (`dSpeed`, `dParticles`, …), read from `HKEY_CURRENT_USER`
in `readRegistry()` and written by the config dialog's `IDOK`. **~124 of those assignments are
unclamped** (Task 11); `src/starfield/starfieldSettings.h` is the model for fixing that — a
`windows.h`-free header with the ranges and a pure `clampToRange`, which is also what makes the
bounds testable without an `HWND`.

`readRegistry` returns early when the key does not exist, which is the case on a fresh CI runner,
so those lines never execute there. That is why coverage reads about five points lower in CI than
on a machine that has run the savers.

### Savers

Each `src/<name>/` is one `.vcxproj` producing one executable: a `.cpp` of the saver's name, a
`.rc` with the config and about dialogs, and `resource.h`. Larger savers add translation units —
`hyperspace` has ten, `skyrocket` seven, `microcosm` six. `helios`, `hyperspace` and `microcosm`
build implicit surfaces through `libs/Implicit` (marching cubes); `skyrocket` is the only one with
sound (OpenAL). Because every module defines `draw()`, `idleProc()` and friends at global scope,
two savers can never share a binary.

### Test harness (`tests/`)

Headless: no GL driver, no window station, no sound card.

- `support/gl_stub.cpp` implements the GL and GLU entry points and **records** them — primitives
  and their vertices, matrix push/pop depth, enable/disable balance, readbacks it could not answer.
  It carries three real 4×4 matrix stacks, so `gluProject` and `glGetFloatv` return coherent
  values; `test_gl_stub.cpp` checks that arithmetic against hand-worked numbers. `GL_STUB_TRACE=1`
  echoes every call to stderr as it happens, which is how three of the open defects were located.
- `support/saver_shim.cpp` defines the eleven globals `rsWin32Saver.h` declares `extern`.
- `support/al_stub.cpp` stands in for OpenAL; linked only by `skyrocket`.
- `support/saver_test_common.h` holds the `SaverFixture` (bring up, discard a warm-up frame,
  guarantee teardown) and the shared frame invariants: `MatrixStackBalanced`, `PrimitivesPaired`,
  `VertexCountsLegal`, `NoInvalidEnums`, `NoEnableStateLeaked`. **Use it rather than copying an
  existing suite** — the duplication gate is real, and the suites hit it for real once.

CMake specifics that are not optional:

- **`WIN32` is a project define, not a compiler builtin** — MSVC predefines `_WIN32` only, and
  every saver body sits inside `#ifdef WIN32`. Without it the translation unit is empty, links
  clean and covers nothing. Each suite has a `SaverBodyWasActuallyCompiled` test guarding this.
- **`_GDI32_`** turns off the `__declspec(dllimport)` on `gl/GL.h`'s entry points so the stub can
  define them; `opengl32.lib` and `glu32.lib` are never linked. **`AL_BUILD_LIBRARY`** is the exact
  analogue for `al.h`.
- **Match `<StackReserveSize>`** if the project sets one: `test_skyrocket` links `/STACK:10000000`
  because setup overflows the linker's 1MB default and dies with `0xC00000FD`.

## Commits

[Conventional Commits](https://www.conventionalcommits.org). Subject is a single line describing
what the commit does — `<type>(<optional scope>): <summary>`, imperative mood, no trailing period.
Types in use here: `feat`, `fix`, `test`, `refactor`, `chore`, `docs`.

```
fix(solarwinds): clear readyToDraw on WM_DESTROY
```

Add a body only when the subject genuinely does not carry it. When present it is a bullet list of
one-liners, **2 to 10 entries**, each a detail of what was done — not a restatement of the subject.
Reference the backlog item where one applies (`Closes Task 14 in docs/MAINTENANCE.md`).

**Never include a `claude.ai/code/session_...` link** — not in a commit trailer, a pull request
description, or a review comment. A `Co-Authored-By: Claude ...` trailer is fine and matches
existing history; the session URL means nothing to anyone reading the log later.

```
test: cover the remaining 7 savers and cut CI coverage time from 40min to 6

- Extend the headless harness to flux, euphoria, helios, lattice, hyperspace, skyrocket and microcosm
- Fix 3 defects the tests surfaced: hyperspace freed textures, skyrocket cleared vector, lattice dead cull block
- Seed the shared PRNG so runs are deterministic — an unseeded run had varied between 40 minutes and 358
- Parallelize the coverage job with ctest -j, cutting the run to under 6 minutes at unchanged coverage
- Closes Task 17 in docs/MAINTENANCE.md
```

## Traps that cost real time

- **`frameTime` is exactly zero unless a test sets it.** Only `idleProc` writes it, from an
  `rsTimer` tick, so a loop of direct `draw()` calls redraws one frozen instant and simulates
  nothing. Drive it (`extern float frameTime;`) and assert something actually moved. This fails
  silently and looks like thoroughness — two `skyrocket` tests once cost 46% of the coverage run
  while launching no rocket at all.
- **Change a setting only while nothing is allocated.** Some savers size their frees from the
  current globals rather than from what they allocated. Always: stop, change, start.
- **None of these savers was written to be restarted in the same process.** Teardown frees memory
  and leaves the counters, flags and function-local statics that index it exactly where they were.
  Six of the nine defects the test rollout found are this one bug wearing different hats. If
  `cleanUp` frees it, `cleanUp` owns resetting whatever counts it.
- **Seven savers carry private `rsRandi`/`rsRandf` copies** with bodies differing from `rsMath.h`'s
  inline versions (Task 12) — a live ODR violation that crashed `starfield` in Release while Debug
  stayed green. **Do not include `<rsMath/rsMath.h>` anywhere that links one of them**, which is
  why `saver_test_common.h` does not, and why only six suites can seed the generator to
  `kTestSeed`. That seed is fixed so a timing or coverage change means the code changed rather
  than the dice.
- **Prefer covering a saver before changing it.** Every one of the nine defects found so far turned
  up that way, and none had surfaced in thirteen years of the savers running.
- Open defects that are not yet fixed are each **pinned by a test asserting current behaviour**, so
  fixing one makes its test fail. That is intended; the test says so in its own comment.
