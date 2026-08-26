# CLAUDE.md

## What this is

Terence M. Welsh's Really Slick Screensavers, imported from SourceForge SVN and kept building on
current Visual Studio. Thirteen OpenGL screensavers under `src/`, each a self-contained Win32
executable, plus `implicitDemo` (a freeglut demo, not a saver). Supporting libraries live in the
`libs` submodule ([rslibs](https://github.com/reallyslickscreensavers/rslibs)).

**`docs/MAINTENANCE.md` is the working backlog — 27 numbered tasks with evidence and `grep`
commands — and the long form of everything below. Read the relevant task before changing anything
in `src/`.** `docs/MAINTENANCE-TOP10.md` derives the top ten; `-rslibs.md` covers the submodule.

## Worktrees — mandatory

- Create worktrees only at `<repo-parent>/<repo-name>.worktrees/<branch>`.
- Never create worktrees inside the repository, including `.claude/worktrees`.
- Base new task worktrees on the latest `origin/main` unless the user requests another ref.
- Initialize submodules separately in every worktree.

## Build and test

```bash
git submodule update --init --recursive   # every worktree needs this, separately
msbuild src\rssavers.sln /p:Configuration=Release /p:Platform=x86 /t:Rebuild
cmake -S tests -B tests/build -A Win32
cmake --build tests/build --config Debug
ctest --test-dir tests/build -C Debug --output-on-failure
```

- **Submodules are per-worktree.** `git worktree add` leaves `libs` and the three `3rdparty/*`
  submodules empty; the failures look like missing headers or libs, not like submodules.
- **Build the solution, never a single `.vcxproj`** — alone, a saver project finds no
  `rsWin32Saverd.lib`.
- **Use `/t:Rebuild`, not `Build`.** Warnings such as `D9035` only appear when source is actually
  recompiled, so a plain `Build` looks clean when it is not.
- **Close anything running from `bin\` first**, or the link fails with `LNK1104`.
- x86 only. Output lands in `bin\`; a post-build event copies each `.exe` to `.scr`.
- One test executable per saver, plus `test_gl_stub` and the settings-only `rssavers_tests`. Narrow
  with `ctest -R Hyperspace`, or one case with `test_hyperspace.exe --gtest_filter=Hyperspace.Restart*`.
- **Coverage is Debug-only** (Release inlining ruins line attribution; OpenCppCoverage needs Debug
  PDBs). `--sources` must be **absolute** — the MSVC CRT's own sources also sit under `\src\`. Never
  add `--modules` (38× slower) or `--timeout`. Full command in `docs/MAINTENANCE.md` under
  "Building and verifying".

`installer/installer.iss` needs Inno Setup; `.github/workflows/release.yml` compiles it with a
pinned `ISCC.exe` and publishes the GitHub Release on a `v*.*.*` tag. `appveyor.yml` still builds
the `Installer` configuration but no longer deploys. `.github/workflows/ci.yml` builds Debug and
Release and runs the tests.

Seven `src/` dirs carry a stale `Makefile` for an `RS_XSCREENSAVER` Linux build with no working
configuration here. Nothing compiles those `#ifdef` blocks, so a green Windows build says nothing.

### Host desktop safety — mandatory

- Never launch `/s`, `/c`, or an arbitrary `/p <hwnd>` on the developer's interactive desktop;
  `/c` disables the foreground owner and a forced exit can leave that app unclickable.
- For dialog QA, use an empty command line (no owner). For renderer smoke tests, use `/w` only.
- Close GUI tests through Cancel, `WM_CLOSE`, or `CloseMainWindow()` and wait for exit. Never use
  `Stop-Process`, `taskkill`, or another forced termination; if graceful close fails, ask the user.
- Run fullscreen tests in Windows Sandbox, a VM, isolated CI, or a separate user session. Use `/p`
  only with a disposable preview host created by the test.
- Prefer headless CTest. If an app is left disabled, close the saver normally or re-enable only its
  known owner with `EnableWindow(hwnd, TRUE)`, then tell the user.

## Architecture

### Saver framework

`libs/rsWin32Saver` owns `WinMain`, the GL context, the message loop and the frame-rate limiter. It
parses the command line (`/c`, `/p <hwnd>`, `/s`, `/w`; `-` accepted for `/`, only the first letter
examined) in `rsWin32SaverSettings.h` and dispatches through a `SaverOps` table. Each saver supplies:

| Symbol | Role |
|---|---|
| `LPCTSTR registryPath` | `Software\Really Slick\<Name>`, the only global the framework mandates |
| `initSaver(HWND)` | allocate everything |
| `draw()` | one frame |
| `idleProc()` | ticks `rsTimer`, sets `frameTime`, calls `draw()` |
| `cleanUp(HWND)` | free everything |
| `LONG screenSaverProc(HWND, UINT, WPARAM, LPARAM)` | `WM_CREATE` → `readRegistry()` + `initSaver`, `WM_DESTROY` → `cleanUp` |
| `setDefaults()` | `(int which)` in solarwinds, euphoria, flux, lattice and microcosm; `()` in the other eight |

- **Three savers deviate** (Task 24): `helios` has `doSaver(HWND)` and no `initSaver`; `skyrocket`
  spells teardown `cleanup` (lowercase u); `flux` returns `LRESULT`. Each is a link error, worked
  around by a one-line shim in the test file rather than by touching saver source.
- Settings are module globals named `d*` (`dSpeed`, `dParticles`, …), read from `HKEY_CURRENT_USER`
  in `readRegistry()` and written by the config dialog's `IDOK`. **~124 are unclamped** (Task 11);
  `src/starfield/starfieldSettings.h` is the model — a `windows.h`-free header with the ranges and a
  pure `clampToRange`, which is what makes the bounds testable without an `HWND`.
- `readRegistry` returns early when the key does not exist, which is the case on a fresh CI runner,
  so coverage reads about five points lower in CI than on a machine that has run the savers.

### Savers

Each `src/<name>/` is one `.vcxproj` producing one executable: a `.cpp` named for the saver, a `.rc`
with the config and about dialogs, and `resource.h`. Larger savers add translation units —
`hyperspace` ten, `skyrocket` seven, `microcosm` six. `helios`, `hyperspace` and `microcosm` build
implicit surfaces through `libs/Implicit` (marching cubes); `skyrocket` alone has sound (OpenAL).
Every module defines `draw()` and `idleProc()` at global scope, so two savers cannot share a binary.

### Test harness (`tests/`)

Headless: no GL driver, no window station, no sound card.

- `support/gl_stub.cpp` implements the GL and GLU entry points and **records** them — primitives and
  vertices, matrix push/pop depth, enable/disable balance, unanswerable readbacks. It carries three
  real 4×4 matrix stacks, so `gluProject` and `glGetFloatv` cohere. `GL_STUB_TRACE=1` echoes every
  call to stderr — that is how three of the open defects were located.
- `support/saver_shim.cpp` defines the 11 globals `rsWin32Saverd.lib` would otherwise own.
- `support/al_stub.cpp` stands in for OpenAL; linked only by `skyrocket`.
- `support/saver_test_common.h` holds the `SaverFixture` (bring up, discard a warm-up frame,
  guarantee teardown) and the shared frame invariants: `MatrixStackBalanced`, `PrimitivesPaired`,
  `VertexCountsLegal`, `NoInvalidEnums`, `NoEnableStateLeaked`. **Use it rather than copying an
  existing suite** — the duplication gate is real, and the suites hit it for real once.

CMake specifics that are not optional:

- **`WIN32` is a project define, not a compiler builtin** — MSVC predefines `_WIN32` only, and every
  saver body sits inside `#ifdef WIN32`. Without it the translation unit is empty, links clean and
  covers nothing. Each suite has a `SaverBodyWasActuallyCompiled` test guarding this.
- **`_GDI32_`** turns off the `__declspec(dllimport)` on `gl/GL.h`'s entry points so the stub can
  define them; `opengl32.lib` and `glu32.lib` are never linked. **`AL_BUILD_LIBRARY`** is the exact
  analogue for `al.h`.
- **Match `<StackReserveSize>`** if the project sets one: `test_skyrocket` links `/STACK:10000000`
  because setup overflows the linker's 1MB default and dies with `0xC00000FD`.

## Commits

[Conventional Commits](https://www.conventionalcommits.org); the types in use are `feat`, `fix`,
`test`, `refactor`, `chore`, `docs`.

```
fix(solarwinds): clear readyToDraw on WM_DESTROY
```

Add a body only when the subject genuinely does not carry it. When present it is a bullet list of
one-liners, **2 to 10 entries**, each a detail of what was done — not a restatement of the subject.
Reference the backlog item where one applies (`Closes Task 14 in docs/MAINTENANCE.md`).

**Never include a `claude.ai/code/session_...` link** — not in a commit trailer, a pull request
description, or a review comment. A `Co-Authored-By: Claude ...` trailer is fine and matches
existing history; the session URL means nothing to anyone reading the log later.

## Traps that cost real time

- **`frameTime` is exactly zero unless a test sets it.** Only `idleProc` writes it, from an `rsTimer`
  tick, so direct `draw()` calls redraw one frozen instant and simulate nothing. Drive it (`extern
  float frameTime;`) and assert something moved — this fails silently and looks like thoroughness.
  Two `skyrocket` tests once cost 46% of the coverage run while launching no rocket at all.
- **Change a setting only while nothing is allocated.** Some savers size their frees from the current
  globals rather than from what they allocated. Always: stop, change, start.
- **None of these savers was written to be restarted in the same process.** Teardown frees memory but
  leaves the counters, flags and function-local statics that index it exactly where they were. Six of
  the nine defects found are this one bug wearing different hats. If `cleanUp` frees it, `cleanUp`
  owns resetting whatever counts it.
- **All thirteen savers share `rsMath.h`'s PRNG** (Task 12, done). `saver_test_common.h` includes
  `<rsMath/rsMath.h>`, and each suite seeds `rsRandGen()` to `kTestSeed` in its fixture's `SetUp`.
  That seed is fixed so a timing or coverage change means the code changed rather than the dice.
  Seven savers used to carry private `rsRandi`/`rsRandf` copies with different bodies — a live ODR
  violation that crashed `starfield` in Release while Debug stayed green. If a private copy ever
  returns, `Starfield.StarLayoutRepeatsForTheSameSeed` is the tripwire.
- **Prefer covering a saver before changing it.** Every one of the nine defects found so far turned
  up that way, and none had surfaced in thirteen years of the savers running.
- Open defects not yet fixed are each **pinned by a test asserting current behaviour**, so fixing one
  makes its test fail. That is intended; the test says so in its own comment.
