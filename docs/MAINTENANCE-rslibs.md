# Maintenance backlog — rslibs (the `libs` submodule)

Companion to [MAINTENANCE.md](MAINTENANCE.md), which covers the `src/` savers in
the parent repository. This file covers the **`rslibs` submodule**
(`github.com/reallyslickscreensavers/rslibs`, checked out at `libs/`).

Layout: 7 library directories, but only 5 have MSVC projects — `rsUtility` is
header-only and `rsXScreenSaver` is Linux-only, both built by Makefile alone.

## Status

**Most of this is done.** L1–L5 and the `rsWin32Saver` half of L8 are merged and
the parent's submodule pointer is at `42d251b` (parent PR #40).

| # | Task | State |
|---|---|---|
| L1 | Debug `EditAndContinue` | **done** — rslibs#20 |
| L2 | `screenSaverConfigureDialog` signature | **done** — rslibs#20, with parent #39 |
| L3 | C++ standard inconsistent | **done** — rslibs#21 |
| L4 | `rsMath.h` PRNG | **done** — rslibs#22 |
| L5 | `readFrameRateLimitFromRegistry` clamp | **done** — rslibs#23 |
| L6 | Frame-rate limit control API | open — breaking, needs coordinated parent edits |
| L7 | `rsText.h` global `to_string` | open — breaking, needs coordinated parent edits |
| L8 | Test coverage gaps | **partial** — `rsWin32Saver` done in rslibs#23; `rsText` and `rsXScreenSaver` deliberately skipped |
| L9 | `impShape` non-virtual destructor | open — contract only, nothing leaks today |

The task descriptions below are kept as written, because the reasoning still
explains *why* each change looks the way it does. Where reality differed from
the prediction, a note says so — L4 in particular did not do what this document
expected.

Only **L6** and **L7** remain, and both are breaking API changes that need
matching edits in the parent in the same sitting.

## Building and verifying

```bash
msbuild rslibs.sln /p:Configuration=Debug /p:Platform=x86 /t:Rebuild
```

```bash
cmake -S tests -B tests/build -A Win32
cmake --build tests/build --config Release
ctest --test-dir tests/build -C Release --output-on-failure
```

Linux, which CI also runs:

```bash
make -j$(nproc)
```

Note rslibs CI (`.github/workflows/ci.yml`) is *better* than the parent's — it
already has a **Linux job** and publishes test results. There is **no SonarCloud
analysis** configured here, so none of the tasks below are gate-driven; they are
correctness and consistency work.

---

# P0 — Blockers for the parent repository

## Task L1 · Debug `EditAndContinue` — DONE (rslibs#20)

**All 5 `.vcxproj`.** In the Debug `<ClCompile>` group:

```xml
<DebugInformationFormat>ProgramDatabase</DebugInformationFormat>
```

```bash
grep -rl "EditAndContinue" */*.vcxproj
```

**Why this is first:** `/ZI` conflicts with `/SAFESEH`, which is on by default
for x86. These are static libraries, so no warning appears when *building*
them — but the object keeps its Edit-and-Continue debug info, and every
executable that **links** the lib gets:

```
rsWin32Saverd.lib(rsWin32Saver.obj) : warning LNK4075: ignoring '/EDITANDCONTINUE' due to '/SAFESEH'
```

This was proven in the parent: after `starfield` was fixed to `/Zi`, the warning
did not disappear — it re-attributed from `starfield.obj` to the lib. **The
parent repository cannot clear this warning on its own.**

Only `rsWin32Saver` triggers it in practice (`rsTextd.lib` does not), but fix
all 5 for consistency. Nothing is lost: the linker already ignores
Edit-and-Continue, so it does not work today.

Note rslibs is already clean of the parent's other two Debug-warning causes —
no `MinimalRebuild` and no Debug LTCG anywhere.

> **Correction.** That last sentence is wrong about LTCG:
> `<LinkTimeCodeGeneration>true</LinkTimeCodeGeneration>` is present in the
> `<Lib>` group of **all five** projects, in both configurations. It is harmless
> — the librarian, not the linker, so it produces no `LNK4075` — which is why it
> was never noticed. The `MinimalRebuild` half of the claim is correct.
>
> Outcome: after this landed, a Debug rebuild of the parent emitted **zero**
> `/EDITANDCONTINUE` warnings, as predicted.

## Task L2 · `screenSaverConfigureDialog` signature — DONE (rslibs#20)

`rsWin32Saver/rsWin32Saver.h:88`:

```cpp
BOOL screenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);
```

and `rsWin32Saver.cpp:485`:

```cpp
parent, (DLGPROC)screenSaverConfigureDialog);
```

Change the declaration to `INT_PTR CALLBACK` and drop the `(DLGPROC)` cast.

**Why:** `BOOL` is not the `DLGPROC` signature, and the cast papers over the
mismatch. Any saver returning a brush from `WM_CTLCOLORSTATIC` through this
proc truncates an `HBRUSH` to `int`. **Currently benign** — the builds are
32-bit x86, where pointers are 32 bits — so this is latent and becomes real the
moment anything targets x64.

**Breaking change for consumers:** all 13 savers define
`screenSaverConfigureDialog` against this declaration, so they must change in
the same coordinated step. Task 5 of the parent's brief (`aboutProc`) is the
matching half and can only be completed once this lands.

---

# P1 — Applicable and worth doing next

## Task L3 · C++ standard is inconsistent in three places — DONE (rslibs#21)

Three separate declarations, all disagreeing:

| Where | Current | Note |
|---|---|---|
| `tests/CMakeLists.txt:4` | `set(CMAKE_CXX_STANDARD 14)` | but GoogleTest v1.17.0 needs 17 |
| 5 sub-Makefiles | `CXXFLAGS ?= -std=c++14 -O3` | explicit C++14 on Linux |
| 5 `.vcxproj` | *(nothing)* | MSVC default, C++14 |

**The tests case is the same trap the parent had.** It does **not** fail:
`CMAKE_CXX_STANDARD` is a floor, and GoogleTest declares
`target_compile_features(... PUBLIC cxx_std_17)` in
`googletest/cmake/internal_utils.cmake`, which propagates so CMake generates
`stdcpp17`. The file simply lies about what it compiles as. If GoogleTest were
ever pinned to an older tag the standard would silently drop to 14 and any
C++17 usage would break for a reason invisible in the CMakeLists.

Fix all three consistently:

- `tests/CMakeLists.txt` → `set(CMAKE_CXX_STANDARD 17)`, with a comment saying
  why.
- Sub-Makefiles (`rsMath`, `Rgbhsl`, `Implicit`, `rsText`, `rsXScreenSaver`) →
  `-std=c++17`. Keep `?=` so the environment can still override.
- 5 `.vcxproj` → add `<LanguageStandard>stdcpp17</LanguageStandard>` to
  **both** configurations. Setting only one leaves Debug and Release on
  different language versions, which is worse than a consistent default.

Risk is low: a scan of the parent plus libs for every construct C++17 *removed*
(`register`, `auto_ptr`, `random_shuffle`, `bind1st/2nd`, `throw()`,
`unary_function`, `binary_function`, `ptr_fun`, `mem_fun`) found **zero** hits.

Doing this here first means the parent's equivalent task inherits a consistent
baseline instead of straddling two standards.

## Task L4 · `rsMath.h` is the origin of the PRNG findings — DONE (rslibs#22)

`rsMath/rsMath.h:47-58`:

```cpp
inline int rsRandi(int x) { return rand() % x; }
inline float rsRandf(float x) { return x * (float(rand()) / float(RAND_MAX)); }
```

**Why it matters beyond style:** this header is the source of the `rand()` usage
in every saver that includes it, and therefore of SonarCloud's `cpp:S2245`
findings in the parent. Fixing it here fixes them everywhere at once, rather
than 13 times.

`starfield` already has the replacement pattern to copy — a function-local
`std::mt19937` seeded from `std::random_device`, with
`std::uniform_real_distribution`. Two things to carry over:

- `rand() % x` is also biased for ranges that do not divide `RAND_MAX` evenly;
  `std::uniform_int_distribution` fixes that as a side effect.
- Sonar flags **every** `<random>` engine under S2245 too, so this does not make
  the finding vanish from a Sonar run — it makes the code correct. In the parent
  the security rating reached **A** regardless.

Check callers before changing signatures: the savers call `rsRandi`/`rsRandf`
heavily, so keep the names and signatures identical and change only the bodies.

> **Correction — the scope claim above is wrong.** "Fixing it here fixes them
> everywhere at once" did not hold. **Six savers define private copies of these
> functions and never include `rsMath.h`**: `cyclone`, `fieldlines`, `flocks`,
> `flux` and `plasma` carry both, `solarwinds` carries `rsRandf`. This change
> reached the seven that do include the header — `euphoria`, `helios`,
> `hyperspace`, `lattice`, `microcosm`, `skyrocket`, `implicitDemo` — and
> `starfield` was already converted. The remaining six are **Task 12** in the
> parent's backlog and still carry their `cpp:S2245` findings — done in the
> parent; see Task 12 in docs/MAINTENANCE.md.
>
> Two things the implementation had to do differently from the plan:
>
> - The engine is **`thread_local`**, not a plain `static`. `microcosm` runs two
>   worker threads and includes this header; a shared `std::mt19937` would be a
>   data race, where the `rand()` it replaced was already per-thread on MSVC and
>   locked in glibc.
> - `rsRandf` scales a canonical `[0, 1)` value rather than using
>   `uniform_real_distribution(0.0f, x)`, the pattern this document pointed at.
>   That form is **undefined for a negative `x`**, and callers pass one:
>   `lattice` computes `rsRandf(150 - dSpeed)` from an unclamped registry value.
>
> `rsRandi(x)` with `x <= 0` was undefined before and is now guarded to return 0,
> reachable through `lattice`'s `rsRandi(11 - dPathrand)`.

## Task L5 · `readFrameRateLimitFromRegistry` does not clamp — DONE (rslibs#23)

`rsWin32Saver/rsWin32Saver.cpp`:

```cpp
int val;
...
result = RegQueryValueEx(skey, "FrameRateLimit", 0, &valtype, (LPBYTE)&val, &valsize);
if (result == ERROR_SUCCESS)
    dFrameRateLimit = val;      // dFrameRateLimit is unsigned int
```

An untrusted registry value goes straight through, and a negative `int`
converts to a huge `unsigned int`. Clamp to a sane range as it is read.

`starfield` has the pattern in `src/starfield/starfieldSettings.h`: take the
value as `unsigned long` and clamp before narrowing, so an oversized `DWORD` is
never converted to `int` first — casting first turns `0xFFFFFFFF` into `-1` and
slips past a naive lower-bound check.

---

# P2 — Larger, needs judgement

## Task L6 · Frame-rate limit control API

`initFrameRateLimitSlider` / `updateFrameRateLimitSlider`
(`rsWin32Saver.cpp:537-555`) hard-code a 0-1000 trackbar plus a text label,
where `0` silently means "unlimited".

`starfield` replaced this in its own dialog with a **checkbox plus an FPS
field**, so the user never has to know `0` is special, while the stored value
keeps its original meaning for backward compatibility. It had to bypass these
helpers entirely to do so, because changing them meant a submodule PR.

Redesigning them here would let all 13 savers adopt the better control. Keep
`0 == unlimited` in the registry regardless — that is the on-disk contract.

## Task L7 · `rsText.h` global `to_string` template

`rsText/rsText.h:49`:

```cpp
template<class T> inline std::string to_string(const T & Value)
{
	std::stringstream ss;
	ss << Value;
	return ss.str();
}
```

A pre-C++11 workaround that predates `std::to_string`, living at **global
scope** in a widely included header. It is why nearly every saver calls
unqualified `to_string(...)`, and it competes with `std::to_string` under ADL.

Removing it is a **breaking change** — every unqualified call site in the
parent must become `std::to_string` in the same coordinated step. `starfield`
already uses `std::to_string`, so it is unaffected and shows the target state.

Consider a deprecation window rather than a hard removal, given the parent
repository is the only consumer.

## Task L8 · Test coverage gaps — PARTIAL

`tests/` currently covers `rsMath`, `rsTrigonometry`, `Rgbhsl`, `Implicit` and
`rsUtility`. **Nothing covers `rsText`, `rsWin32Saver` or `rsXScreenSaver`.**

`rsWin32Saver` is the highest-value gap: it owns registry reading, the frame
rate limit and the dialog plumbing — exactly the logic Tasks L5 and L6 change.
Follow the parent's approach and extract the decidable logic into a
`windows.h`-free header so it is testable without an `HWND`; the Win32 calls
themselves are not unit-testable and are not worth mocking.

**`rsWin32Saver` is done** (rslibs#23). `rsWin32SaverSettings.h` holds the frame
rate bounds and clamp, the command-line parser, and a `SaverOps` seam that makes
`WinMain`'s dispatch testable — `WinMain` itself cannot be linked into a test
binary, since rsWin32Saver is a static library with no executable and `WinMain`
would fight `gtest_main`'s `main`. Tests went 198 → 226.

**`rsText` and `rsXScreenSaver` are deliberately left**, and this is the part
worth not re-litigating:

- `rsText`'s only non-OpenGL surface is the global `to_string` template that
  **Task L7 exists to delete**. Testing it would entrench what the next task
  removes. `rsText::draw` needs a live GL context.
- `rsXScreenSaver` is vendored Xlib compatibility plumbing (`vroot.h`) needing
  X11 types and a display connection. There is nothing decidable to test without
  mocking Xlib, which this task itself says is not worth it.

So L8 should be considered **closed in practice**, not pending, unless L7 changes
what `rsText` looks like.

## Task L9 · `impShape` has virtuals and a non-virtual destructor

`impShape` (`Implicit/impShape.h:56`) declares `~impShape() {}` alongside three
virtual functions, and every consumer holds shapes by base pointer. Deleting one
through an `impShape*` is undefined behaviour — formally, not just by lint.

Found from the parent while fixing its Task 26: `microcosm`'s gizmos own their
shapes in a `ShapeVector` and now delete them from one place, which made the
question unavoidable. Nothing leaks today, because no `impShape` subclass owns
memory — `impSphere`, `impTorus`, `impKnot`, `impCapsule`, `impEllipsoid` and
`impRoundedHexahedron` hold only scalars, and their destructors are empty. So
this is a correctness-of-contract fix, not a live defect, which is why the
parent recorded it here instead of working around it.

The fix is one `virtual`. Watch the SSE trap below: `operator new` / `operator
delete` are overridden on this class for 16-byte alignment, and a virtual
destructor adds a vptr to every shape, so re-check the alignment assumption
under `__SSE__` rather than assuming it still holds. MSVC does not define
`__SSE__`, so the parent's Windows build never takes that path.

---

# Traps

- **`Implicit/impShape.h:86-100` overrides `operator new` / `operator delete`
  deliberately**, to force 16-byte alignment for SSE `__m128` members. A
  "modernise raw new/delete" pass **must not** touch it. It is guarded by
  `#ifdef __SSE__` and uses `_aligned_malloc` on Windows, `memalign`
  elsewhere.
- The only `gets` hit from a naive unsafe-function scan is a false positive —
  the word appears in a comment in that same header.
- Static libraries hide linker-flag problems. Task L1's warning only appears in
  the consuming executable, so **verify against the parent repository**, not
  just a green `rslibs.sln` build.
- Changing a public header here breaks the parent silently until the submodule
  pointer is bumped. For Tasks L2 and L7, prepare the parent-side change in the
  same sitting.

---

# Not applicable here

Carried over from the parent's brief for completeness:

- **Post-build copy with spaced paths** — rslibs has no `PostBuildEvent` copy
  commands. Confined to `src/`.
- **`resource.h` include style** — no `resource.h` in rslibs.
- **`aboutProc` truncation** — no About dialogs here. The related issue is
  Task L2.
- **Mutable global encapsulation (`cpp:S5421`)** — rslibs' globals
  (`isSuspended`, `kStatistics`, `dFrameRateLimit`, `mainInstance`,
  `registryPath`, `xdisplay`, `xwindow`) are **deliberately** `extern`: they are
  the documented contract between the framework and each saver. Encapsulating
  them would break every consumer. Leave them.
- **SonarCloud duplication** — no Sonar project configured for rslibs.

> **Correction.** rslibs *does* have SonarCloud analysis: it ran on every one of
> rslibs#20 through #23 as the "SonarCloud Code Analysis" check. It failed once,
> on #22, at `new_security_rating` C — `cpp:S2245` fires on `std::mt19937` just
> as it did on `rand()`, exactly as Task L4 predicted. Those two findings were
> accepted as *Safe*, which is the right disposition for a screensaver choosing
> star positions, and the gate has been green since.

---

# Suggested order

Kept for the record; L1–L5 and L8's `rsWin32Saver` half were done in this order
and it worked.

1. ~~**L1** and **L2** together~~ — done in rslibs#20, with parent #39.
2. ~~**L3**, then **L4**~~ — done in rslibs#21 and #22.
3. ~~**L5**, optionally with **L8**'s `rsWin32Saver` tests~~ — done together in
   rslibs#23. Pairing them was right: L5's extraction is what made the logic
   testable.
4. **L6**, **L7** and **L9** — still to do. L6 and L7 are breaking API changes
   needing coordinated parent-side edits; L9 is one keyword, but read its note on
   the SSE alignment override first.
   L5 already moved the frame-rate bounds into shared constants, so L6 has its
   groundwork.
