/*
 * Tests for the helios saver.
 *
 * helios.cpp is compiled into this binary against the recording GL stub, so the
 * real draw path runs headless. Shared scaffolding - the fixture and the frame
 * invariants - lives in support/saver_test_common.h; what is here is what is
 * specific to helios.
 */

#include "support/saver_test_common.h"

// Seeding is what makes the isosurface assertions below a fixed outcome
// rather than a probable one.

#include "resource.h"
#include "heliosSettings.h"

// helios.cpp has no header; its contract with the framework is by name. See the
// note on cpp:S5421 in test_fieldlines.cpp - these are declarations, not
// definitions.
extern int dIons;
extern int dSize;
extern int dEmitters;
extern int dAttracters;
extern int dSpeed;
extern int dCameraspeed;
extern int dSurface;
extern int dBlur;
extern int readyToDraw;

// The sphere count surfaceFunction sums over, written where doSaver allocates
// spheres (helios.cpp:863). Was a function-local static that a restart with
// fewer emitters/attracters left indexing spheres past its end - Task 20 in
// docs/MAINTENANCE.md.
extern int surfacePoints;

// Seconds since the last frame. idleProc sets it from an rsTimer
// (helios.cpp:740), but the tests call draw() directly, so it stays at its
// initial 0.0f unless a test drives it - and at zero nothing evolves.
extern float frameTime;

void setDefaults();
void readRegistry();
void initControls(HWND hdlg);
LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

// helios is the only saver of the thirteen that does not have an initSaver.
// Its window procedure calls doSaver instead (helios.cpp:1134), so the shared
// fixture's call is satisfied here rather than by the saver.
void doSaver(HWND hwnd);

void initSaver(HWND hwnd) { doSaver(hwnd); }

namespace {

// helios spreads the whole ion release over 120 s
// (releaseTime += 120.0f / float(dIons), helios.cpp:546), so any step past 120
// releases every ion whatever dIons is, and whatever releaseTime was left
// holding. Used to drive a restart past the entire release schedule so the
// fixed and unfixed paths are told apart by ionsReleased, not by timing.
constexpr float kReleaseEverythingStep = 200.0f;

// preinterp += frameTime * float(dSpeed) * interpconst (helios.cpp:556)
// advances 0.01 per second at dSpeed 10 and interpconst 0.001, so only a step
// past ~315 s carries preinterp over PI and re-runs setTargets
// (helios.cpp:559-566); it must also clear the 10 s wait. Without it the
// freshly allocated emitters are positioned from uninitialised
// oldpos/targetpos (rsVec's default constructor leaves v[] uninitialised,
// libs/rsMath/rsVec.cpp:25) and no isosurface forms.
constexpr float kPatternStep = 400.0f;

// helios has a single set of defaults rather than presets, and no argument.
class Helios : public savertest::SaverFixture {
protected:
    void SetUp() override {
        // Before setDefaults, and before anything reaches initSaver. helios
        // draws targets and colours from rsMath's generator (helios.cpp:114,
        // 131, 151, 157, 160, ...); seeding it is what makes the isosurface
        // assertions in SurfaceModeBuildsAndDrawsTheMesh and
        // RestartWithFewerSpheresRebuildsTheSurface a fixed outcome rather
        // than a probable one - see the note on kTestSeed in
        // saver_test_common.h.
        rsRandGen().seed(savertest::kTestSeed);

        setDefaults();
        // The shipped default of 1500 ions makes every case here several times
        // slower for no extra coverage; the count itself is asserted below.
        dIons = 60;
    }
};

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(HeliosHarness, SaverBodyWasActuallyCompiled) {
    // Guards the WIN32-define trap. MSVC predefines _WIN32, not WIN32, and the
    // whole saver sits inside #ifdef WIN32; without it this links against an
    // empty translation unit and every other test passes vacuously.
    setDefaults();
    EXPECT_EQ(dIons, 1500);
    EXPECT_EQ(dSize, 10);
    EXPECT_EQ(dEmitters, 3);
    EXPECT_EQ(dAttracters, 3);
    EXPECT_EQ(dSurface, 1);
}

TEST(HeliosHarness, DefaultsSitInsideTheDeclaredRanges) {
    // The header declares the ranges and the saver picks the defaults; nothing
    // else checks that the two agree.
    setDefaults();
    EXPECT_TRUE(savertest::SettingsWithinDeclaredRanges({
        savertest::Ranged("dIons", dIons, heliosSettings::kIons),
        savertest::Ranged("dSize", dSize, heliosSettings::kSize),
        savertest::Ranged("dEmitters", dEmitters, heliosSettings::kEmitters),
        savertest::Ranged("dAttracters", dAttracters, heliosSettings::kAttracters),
        savertest::Ranged("dSpeed", dSpeed, heliosSettings::kSpeed),
        savertest::Ranged("dCameraspeed", dCameraspeed, heliosSettings::kCameraspeed),
        savertest::Ranged("dSurface", dSurface, heliosSettings::kSurface),
        savertest::Ranged("dBlur", dBlur, heliosSettings::kBlur),
    }));
}

// --- a frame ---------------------------------------------------------------

TEST_F(Helios, FrameLeavesTheMatrixStackBalanced) {
    start();
    draw();
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

TEST_F(Helios, FramePairsBeginAndEnd) {
    start();
    draw();
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

TEST_F(Helios, PrimitiveVertexCountsAreLegal) {
    start();
    draw();
    EXPECT_TRUE(savertest::VertexCountsLegal());
}

TEST_F(Helios, DoesNotLeakEnableState) {
    start();
    draw();
    EXPECT_TRUE(savertest::NoEnableStateLeaked());
}

TEST_F(Helios, ReadsBackNoInvalidEnums) {
    start();
    draw();
    EXPECT_TRUE(savertest::NoInvalidEnums());
}

TEST_F(Helios, DrawsIonsThroughTheBillboardDisplayList) {
    // Each ion is a textured billboard compiled into list 1 in doSaver
    // (helios.cpp:824-840) and called once per released ion (helios.cpp:275).
    //
    // The loop runs to ionsReleased, not dIons: helios lets ions out gradually
    // over time (helios.cpp:536-541), so a frame draws however many have been
    // released so far, never more than dIons.
    start();
    draw();

    const int calls = glstub::trace().countCalls("glCallList");
    EXPECT_GT(calls, 0);
    EXPECT_LE(calls, dIons);
}

TEST_F(Helios, ReleasesMoreIonsAsTimePasses) {
    // Time has to be driven for this to mean anything. draw() spends frameTime
    // (helios.cpp:537) but only idleProc sets it (helios.cpp:740), so a loop of
    // bare draw() calls redraws one frozen instant.
    //
    // This test used to do exactly that and assert GE, which a frozen clock
    // satisfies by leaving the two counts equal - it passed while testing
    // nothing. The assertion below is strict for that reason.
    // The step is coarse on purpose. helios spreads the whole ion release over
    // two minutes - releaseTime += 120.0f / dIons at helios.cpp:542 - so with
    // the 60 ions this fixture uses that is two seconds each. A realistic 1/30
    // second frame would need hundreds of iterations to release a single one;
    // half-second steps reach several in twenty.
    constexpr float kCoarseStep = 0.5f;

    start();
    draw();
    const int early = glstub::trace().countCalls("glCallList");

    for (int frame = 0; frame < 20; ++frame) {
        frameTime = kCoarseStep;
        draw();
    }
    glstub::reset();
    frameTime = kCoarseStep;
    draw();
    const int later = glstub::trace().countCalls("glCallList");

    EXPECT_GT(later, early) << "no ions released - is frameTime being driven?";
    EXPECT_LE(later, dIons) << "the release loop is bounded by dIons";
}

// --- settings change what is drawn -----------------------------------------

// dSurface adds a marching-cubes shell around the emitters. impSurface draws it
// with glDrawElements rather than glBegin, so it lands in arrayPrimitives; the
// GL_TRIANGLE_STRIP that shows up in a frame is the blur quad, not the mesh.
//
// These are two separate cases rather than one before/after, because helios
// keeps its simulation in function-local statics inside draw() - interp, wait,
// newTarget (helios.cpp:551-572) - that survive cleanUp, so a frame drawn with
// dSurface off leaves the emitters wherever they were. ctest runs every case in
// its own process, which is what makes the split work. See
// RestartResetsTheIonReleaseCount for the same root cause.
//
// Task 20 fixed surfaceFunction's cached sphere count (formerly the
// function-local static `points`, now the file-scope surfacePoints assigned
// where doSaver allocates spheres), so the mesh itself can now be asserted
// rather than only the branch that builds it. frameTime is driven to
// kPatternStep before start() so the fixture's warm-up frame re-runs
// setTargets and the emitters actually form a cluster - see kPatternStep above
// for why, and Task 20's note in docs/MAINTENANCE.md on rsVec's uninitialised
// default construction.

TEST_F(Helios, SurfaceModeBuildsAndDrawsTheMesh) {
    // The branch turns on sphere-map texture generation around the mesh
    // (helios.cpp:636-659), which is observable no matter where the emitters
    // have drifted to.
    stop();
    dSurface = 0;
    start();
    draw();
    EXPECT_EQ(glstub::trace().countEnables(GL_TEXTURE_GEN_S), 0);
    EXPECT_EQ(glstub::trace().countCalls("glDrawElements"), 0);

    stop();
    dSurface = 1;
    frameTime = kPatternStep;  // re-run setTargets on the warm-up frame
    start();
    draw();
    EXPECT_EQ(glstub::trace().countEnables(GL_TEXTURE_GEN_S), 1);
    EXPECT_EQ(glstub::trace().countEnables(GL_TEXTURE_GEN_T), 1);
    EXPECT_TRUE(savertest::NoEnableStateLeaked())
        << "the branch must switch texture generation back off";

    // impSurface::draw returns before glDrawElements when index_offset == 0
    // (libs/Implicit/impSurface.cpp:188), so a non-zero count is exactly "a
    // mesh came out" - not just "the branch that builds one was taken".
    EXPECT_GT(glstub::trace().countCalls("glDrawElements"), 0)
        << "impSurface draws the marching-cubes mesh through vertex arrays";
    EXPECT_GT(glstub::trace().totalArrayVertices(), 0u);
}

TEST_F(Helios, RestartWithFewerSpheresRebuildsTheSurface) {
    // Task 20, the other half of the fix: surfaceFunction used to sum over a
    // function-local static `points`, cached once from dEmitters + dAttracters
    // and never updated, while doSaver reallocated spheres to whatever the
    // current, possibly smaller, settings were. A restart with fewer spheres
    // then summed spheres past the end of the array on every sample of the
    // volume. This pins the fix: surfacePoints must equal the count the
    // spheres array was actually allocated with, and a mesh must still come
    // out at the smaller count.
    //
    // 2 emitters + 1 attracter rather than 1 + 1: makeSurface's crawl only
    // ever steps ++i and abandons a crawl point once i >= w with the cube
    // still fully inside (libs/Implicit/impCubeVolume.cpp:250-256);
    // crawlfromsides is false by default and helios never turns it on, so
    // there is no fallback scan. At 2 + 1 the scale factor is 1/sqrt(5),
    // giving an emitter thickness of 179 and a lone-sphere iso radius of about
    // 283 against a 875 half-extent and a maximum centre of 500 - 92 units of
    // margin, versus under one cube at 1 + 1. Never use dEmitters = 0;
    // setTargets divides by counts derived from it (helios.cpp:291, 305, 321).
    stop();
    dEmitters = 3;
    dAttracters = 3;
    frameTime = kPatternStep;
    start();  // cycle 1: dSurface 1, 3 emitters + 3 attracters (6 spheres)

    stop();
    dEmitters = 2;  // three spheres where cycle 1 allocated six
    dAttracters = 1;
    frameTime = kPatternStep;
    start();
    draw();

    EXPECT_EQ(surfacePoints, dEmitters + dAttracters)
        << "surfaceFunction must sum the array doSaver just allocated";
    EXPECT_GT(glstub::trace().countCalls("glDrawElements"), 0)
        << "the mesh is rebuilt at the smaller sphere count";
}

TEST_F(Helios, BlurDrawsTheFadingQuadInsteadOfClearing) {
    // With dBlur the frame is dimmed by drawing a translucent screen-filling
    // strip in an ortho projection rather than clearing (helios.cpp:602-620).
    stop();
    dBlur = 0;
    start();
    draw();
    EXPECT_EQ(countPrimitives(GL_TRIANGLE_STRIP), 0);
    EXPECT_GT(glstub::trace().countCalls("glClear"), 0);

    stop();
    dBlur = 50;
    start();
    draw();
    EXPECT_EQ(countPrimitives(GL_TRIANGLE_STRIP), 1);
    EXPECT_TRUE(savertest::MatrixStackBalanced())
        << "the blur path pushes on both the projection and modelview stacks";
}

TEST_F(Helios, RestartResetsTheIonReleaseCount) {
    // Task 20: ionsReleased used to be a function-local static inside draw()
    // (formerly helios.cpp:452), never reset across a restart in the same
    // process, while doSaver reallocated ilist to the current, possibly
    // smaller, dIons. A restart with a SMALLER dIons then indexed ilist past
    // its end - the old pinning test here could not observe that, because its
    // EXPECT_GT(calls, 0) was satisfied by the un-reset releaseTime
    // (helios.cpp:453 at the time), not by ionsReleased: it passed identically
    // whether the counter was reset or not.
    //
    // This version discriminates. One glCallList is emitted per released ion
    // (helios.cpp:275; kStatistics is 0 in the shim, so nothing else calls
    // it), which makes ionsReleased observable. kReleaseEverythingStep drives
    // both the first cycle and the restart's warm-up frame past the entire
    // 120 s release schedule, so the fixed path always reaches exactly dIons
    // ions whatever releaseTime carried in, while a regressed path that never
    // resets the counter would see `60 < 20` (dIons already "exceeded") and
    // release nothing, drawing the stale 60 over a 20-element array instead.
    //
    // A regression here most likely fails this assertion cleanly (40 stale
    // entries past a 20-element array usually stays inside the heap block),
    // but could instead access-violate reading ilist out of bounds during the
    // cycle-2 warm-up; ctest reports that as a failed test too, so it is not
    // silent.
    start();  // fixture sets dIons = 60
    const int firstCycleIons = dIons;
    frameTime = kReleaseEverythingStep;
    draw();
    ASSERT_EQ(glstub::trace().countCalls("glCallList"), firstCycleIons)
        << "the first cycle must release every ion, or the restart proves nothing";

    stop();
    dIons = 20;  // fewer than the first cycle released
    start();     // warm-up runs at the same big step
    draw();

    EXPECT_EQ(glstub::trace().countCalls("glCallList"), dIons)
        << "the release counter restarts from zero; this loop indexes ilist";
}

// --- framework entry points ------------------------------------------------

TEST_F(Helios, IdleProcSkipsDrawingWhenNotReady) {
    start();
    readyToDraw = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().countCalls("glCallList"), 0);
    readyToDraw = 1;
}

TEST(HeliosFramework, ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy) {
    setDefaults();
    dIons = 30;
    readyToDraw = 0;

    screenSaverProc(testsupport::hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(readyToDraw, 1);

    screenSaverProc(testsupport::hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(readyToDraw, 0);
}

TEST(HeliosFramework, ReadRegistryLeavesEverySettingInsideItsDeclaredRange) {
    // Read-only: setDefaults runs first and the function returns early if the
    // key is absent, so this cannot disturb the machine. That early return also
    // means the clamp itself covers little where the saver has never stored
    // settings, CI included - see the note in test_cyclone.cpp.
    setDefaults();
    readRegistry();

    EXPECT_GT(dEmitters, 0) << "so is the emitter array";
    EXPECT_GT(dAttracters, 0);
    EXPECT_GT(dSpeed, 0);
    EXPECT_TRUE(savertest::SettingsWithinDeclaredRanges({
        savertest::Ranged("dIons", dIons, heliosSettings::kIons),
        savertest::Ranged("dSize", dSize, heliosSettings::kSize),
        savertest::Ranged("dEmitters", dEmitters, heliosSettings::kEmitters),
        savertest::Ranged("dAttracters", dAttracters, heliosSettings::kAttracters),
        savertest::Ranged("dSpeed", dSpeed, heliosSettings::kSpeed),
        savertest::Ranged("dCameraspeed", dCameraspeed, heliosSettings::kCameraspeed),
        savertest::Ranged("dSurface", dSurface, heliosSettings::kSurface),
        savertest::Ranged("dBlur", dBlur, heliosSettings::kBlur),
    }));
}

// --- dialog procedures -----------------------------------------------------

TEST(HeliosDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_TRUE(savertest::AboutProcColoursTheWebPageLabel(aboutProc));
}

TEST(HeliosDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(aboutProc));
}

TEST(HeliosDialogs, InitControlsRunsWithoutADialog) {
    setDefaults();
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(HeliosDialogs, ConfigureDialogHandlesTheStandardMessages) {
    EXPECT_TRUE(savertest::ConfigureDialogInitialisesAndCancels(screenSaverConfigureDialog));
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(screenSaverConfigureDialog));
}

TEST(HeliosDialogs, ConfigureDialogRestoresDefaults) {
    setDefaults();
    const int defaultIons = dIons;
    dIons = 99;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS, 0);

    EXPECT_EQ(dIons, defaultIons);
}
