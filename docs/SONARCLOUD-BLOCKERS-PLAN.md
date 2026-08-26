# SonarCloud blocker remediation plan

Analysis date: 2026-08-26
Baseline: `origin/main` at `50cff2f0b828c9d6966d7a25770d91093b21697b`

SonarCloud reports six blocker-severity issues on `main`. Two are `cpp:S3519`
findings in Cyclone and four are `cpp:S5020` findings for legacy `srand` calls.
The source audit also found the same live `srand` call in Hyperspace and
Microcosm. SonarCloud does not currently list those two as open `cpp:S5020`
issues, but the same reasoning applies, so the cleanup should remove all six
active calls rather than encode the dashboard's incomplete set into the code.

## Assessment

| Issue | Location | Assessment | Planned disposition |
|---|---|---|---|
| `AaAbE8bbB1rsUSwAAOq5` | `src/cyclone/cyclone.cpp:149` | Not reachable in production | Resolve as false positive in SonarCloud |
| `AaAbE8bbB1rsUSwAAOq6` | `src/cyclone/cyclone.cpp:157` | Not reachable in production | Resolve as false positive in SonarCloud |
| `AZ_OYiEds5KeDF11J9ZH` | `src/euphoria/euphoria.cpp:474` | Applicable, but behaviorally redundant | Remove the call and unused header |
| `AZ_OYiFms5KeDF11J9d_` | `src/helios/helios.cpp:760` | Applicable, but behaviorally redundant | Remove the call and unused header |
| `AZ_OYiBRs5KeDF11J9W2` | `src/lattice/lattice.cpp:683` | Applicable, but behaviorally redundant | Remove the call and unused header |
| `AZ_OYh90s5KeDF11J9PC` | `src/skyrocket/skyrocket.cpp:927` | Applicable, but behaviorally redundant | Remove the call and unused header |
| Not currently reported | `src/hyperspace/hyperspace.cpp:544` | Same live, behaviorally redundant call | Remove it in the same cleanup |
| Not currently reported | `src/microcosm/microcosm.cpp:938` | Same live, behaviorally redundant call | Remove it in the same cleanup |

### Why the Cyclone accesses are safe

The constructor allocates `dComplexity + 3` pointer slots and accesses indices
from `0` through `dComplexity + 2`. With the declared `dComplexity` range of
`1..10`, all reported accesses are in bounds. A failing path requires a
negative value.

Production initialization prevents that path:

1. `screenSaverProc(WM_CREATE)` calls `readRegistry()` before `initSaver()`.
2. `readRegistry()` first installs the default value `3`.
3. A stored registry value is accepted only as a correctly sized `REG_DWORD`
   and is clamped against `cycloneSettings::kComplexity` (`1..10`).
4. The configuration dialog constrains its slider to the same range; more
   importantly, the next saver launch revalidates the persisted value rather
   than trusting the dialog or registry.

The analyzer does not carry this global invariant through the lifecycle and
constructor call. The existing `CycloneBlockerGuard` tests cover restoration
of the default after a corrupted in-memory value and the `WM_CREATE` ordering;
the shared `SaverSettings` boundary tests exercise every declared range,
including `kComplexity`, with undersized and oversized values; and
`SettingsClampWiring` pins the source-level pairing between `dComplexity` and
`kComplexity`. Adding casts or a second clamp at the access site would
duplicate an invariant already enforced and tested at the input boundary, so
it is not recommended.

### Why the `srand` calls should be deleted

All six savers obtain random values through `rsRandi` and `rsRandf` from
`libs/rsMath/rsMath.h`. Those helpers now use a self-seeded, thread-local
`std::mt19937`; their documentation explicitly states that callers' `srand`
calls have no effect. The remaining calls therefore seed an unused C PRNG and
misrepresent how randomness is initialized.

Do not replace the calls with another explicit seed. Delete each call, its
adjacent obsolete seeding comment where present, and the now-unused `<time.h>`
include in the same six translation units. The commented-out `srand(0)` in
`microcosm/mirrorBox.cpp` is not executable and is outside this change.

## Implementation sequence

1. In `euphoria.cpp`, `helios.cpp`, `hyperspace.cpp`, `lattice.cpp`,
   `microcosm.cpp`, and `skyrocket.cpp`, remove
   `srand((unsigned)time(NULL));`, any comment that describes it as seeding the
   active generator, and `<time.h>` after confirming that translation unit has
   no other `time` use.
2. Add `tests/tools/check-legacy-prng.cmake` and register it as the
   `NoLegacyCPrngCalls` CTest. It scans active parent-project source lines for
   direct `rand()` or `srand()` calls, because SonarCloud missed two of the six
   live calls found during this audit.
3. Leave the Cyclone allocation and indexing logic unchanged. On the existing
   main-branch issues, add the evidence above and resolve both as false
   positives. This is a SonarCloud project action requiring the appropriate
   issue-management permission; it is separate from the code pull request.
4. In the code pull request, update `docs/MAINTENANCE.md` only where it describes
   the underlying RNG or Cyclone disposition. Do not publish a speculative
   dashboard count before main has been reanalyzed.
5. Merge the code change, wait for a successful main-branch SonarCloud
   analysis, and then confirm the four open `cpp:S5020` issues close
   automatically and the blocker filter returns zero open or confirmed issues.
   If the dashboard differs, investigate the analysis result before updating
   documented counts.

If project policy or permissions prevent a false-positive disposition for the
Cyclone issues, do not add casts or isolated checks merely to silence the
analyzer. The coherent code fallback is to give each `cyclone` instance a
validated `complexity` member captured at construction, then use that member
consistently for its allocation sizes and every Cyclone/particle index derived
from complexity. That is a larger hardening refactor and should be tested as
such; it is not the preferred fix while the existing input-boundary invariant
is valid and documented.

## Verification

Before opening a pull request:

- Confirm `rg -n "^[[:space:]]*srand[[:space:]]*\\(" src` returns no active
  calls. The anchored expression intentionally ignores the commented example
  in `microcosm/mirrorBox.cpp`.
- Confirm the six edited translation units no longer include `<time.h>` and
  contain no other `time(...)` use.
- Build all six edited saver targets and their tests in Debug and Release.
- Run the full CTest suite in both configurations, including
  `CycloneBlockerGuard.*`, `SaverSettings.*`, `SettingsClampWiring`, and
  `NoLegacyCPrngCalls`.
- Run the normal solution build for x86 Debug and Release, matching CI.
- After the merged commit receives a successful main-branch SonarCloud
  analysis, confirm all four `cpp:S5020` issues are closed and both `cpp:S3519`
  issues retain their documented false-positive disposition.

## Implementation status and validation

The code and test changes in this worktree complete steps 1, 2, and 4 above.
Validation completed successfully:

- The full CTest suite passed in Release (334/334) and Debug (334/334),
  including `NoLegacyCPrngCalls`, `CycloneBlockerGuard.*`,
  `SaverSettings.*`, and `SettingsClampWiring`.
- The complete x86 solution built in Release and Debug with zero warnings and
  zero errors.
- The source audit found no remaining active `rand()` or `srand()` calls and
  no remaining `<time.h>` or `time(...)` dependency in the six edited saver
  translation units.

Steps 3 and 5 remain post-change project actions: a maintainer with appropriate
SonarCloud permissions must classify the two Cyclone findings as false
positives, and the four reported `cpp:S5020` findings can only be confirmed
closed after this change is merged and a successful analysis of `main`
finishes.
