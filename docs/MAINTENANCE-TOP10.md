# Maintenance top 10 — checklist

Derived from `docs/MAINTENANCE.md`'s own "Priority and order" section, verified
against the current tree on 2026-08-16. Check items off as they land.

## Do first

- [x] **Task 20 — `helios` out-of-bounds read.** [src/helios/helios.cpp:440](../src/helios/helios.cpp#L440)
  and [:452](../src/helios/helios.cpp#L452) hold two function-local statics
  (`points`, `ionsReleased`) that survive a restart in the same process while
  the arrays they index (`spheres`, `ilist`) get resized. `points` in
  particular drives an out-of-bounds read on every sample of a 70×70×70 volume
  if `dEmitters`/`dAttracters` shrink on restart. Only remaining open item
  that's an OOB *read* rather than a latent trap; fix is hoisting both statics
  to file scope and assigning them in `doSaver` (same shape as the Task 18
  fix). Confirmed still open by grep on 2026-08-15.

## Top 10, in priority order

1. [x] **Task 20** — helios OOB read — `src/helios/helios.cpp:440,452`
2. [x] **Task 10 remainder** — `lattice.cpp:700` uninitialised camera-ctor
       field + torus-guard `cpp:S836` + 5 `skyrocket/particle.cpp`
       garbage-value findings — fixed in #53, `particle.cpp` coverage raised
       17.6% → 95.3%. Task 10 itself stays **PARTIAL** in `docs/MAINTENANCE.md`:
       `cpp:S6232` ×4, `cpp:S1763` ×2 and `cpp:S836` ×2 remain, all
       lower-severity and untouched by #53.
3. [x] **Task 21** — `hyperspace` unguarded `glActiveTextureARB` — moved the
       three calls (now `src/hyperspace/hyperspace.cpp:245,247,249`) inside
       the existing `if(dShaders)`
4. [ ] **Task 15** — `fieldlines` nested `glBegin` loses per-segment line
       widths — `src/fieldlines/fieldlines.cpp:247-260`
5. [ ] **Task 25** — `skyrocket` won't start without `OpenAL32.dll` —
       `src/skyrocket/skyrocket.vcxproj`; needs `LoadLibrary`-based lazy load
       or a shipped redistributable
6. [ ] **Task 2 remainder** — delete `MinimalRebuild` (13 files) and Debug
       `LinkTimeCodeGeneration` (9 files) —
       `grep -rl "MinimalRebuild" src --include=*.vcxproj`
7. [ ] **Task 3** — declare C++17 explicitly in 13 `.vcxproj` —
       `grep -rL "LanguageStandard" src/*/*.vcxproj`
8. [ ] **Task 13** — 26 clear-text `http://` URLs —
       `grep -rn "http://" src --include=*.cpp --include=*.h --include=*.rc`;
       check `https://` works before switching
9. [ ] **Task 11** — ~124 unclamped registry values across 13 savers —
       model is `src/starfield/starfieldSettings.h`
10. [ ] **Task 12** — delete 7 private PRNG copies (ODR violation, confirmed
        Debug/Release divergence) —
        `grep -rln "inline int rsRandi\|inline float rsRandf\|inline std::mt19937& rsRandGen" src --include=*.cpp`

## Just outside the top 10

- [ ] **Task 26** — `microcosm` appends its gizmo list instead of clearing it
      first (tab instead of newline) — `src/microcosm/microcosm.cpp:979`.
      One-character fix for the split; freeing gizmos in `cleanUp` is the
      larger half since it frees nothing today.

Full detail, evidence and `grep` commands for every item: `docs/MAINTENANCE.md`.
