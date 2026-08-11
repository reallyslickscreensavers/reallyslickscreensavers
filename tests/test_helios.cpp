/*
 * Tests for the helios saver.
 *
 * helios.cpp is compiled into this binary against the recording GL stub, so the
 * real draw path runs headless. Shared scaffolding - the fixture and the frame
 * invariants - lives in support/saver_test_common.h; what is here is what is
 * specific to helios.
 */

#include "support/saver_test_common.h"


#include "resource.h"

// helios.cpp has no header; its contract with the framework is by name. See the
// note on cpp:S5421 in test_fieldlines.cpp - these are declarations, not
// definitions.
extern int dIons;
extern int dSize;
extern int dEmitters;
extern int dAttracters;
extern int dSpeed;
extern int dSurface;
extern int dBlur;
extern int readyToDraw;

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

// helios has a single set of defaults rather than presets, and no argument.
class Helios : public savertest::SaverFixture {
protected:
    void SetUp() override {
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
    start();
    draw();
    const int early = glstub::trace().countCalls("glCallList");

    for (int frame = 0; frame < 20; ++frame) draw();
    glstub::reset();
    draw();
    const int later = glstub::trace().countCalls("glCallList");

    EXPECT_GE(later, early);
    EXPECT_LE(later, dIons) << "the release loop is bounded by dIons";
}

// --- settings change what is drawn -----------------------------------------

// dSurface adds a marching-cubes shell around the emitters. impSurface draws it
// with glDrawElements rather than glBegin, so it lands in arrayPrimitives; the
// GL_TRIANGLE_STRIP that shows up in a frame is the blur quad, not the mesh.
//
// These are two separate cases rather than one before/after, because helios
// keeps its simulation in function-local statics inside draw() - interp, wait,
// newTarget, and surfaceFunction's own `points` (helios.cpp:440, 452, 520-568).
// Those survive cleanUp, so a frame drawn with dSurface off leaves the emitters
// somewhere that produces no isosurface when it is turned back on in the same
// process. ctest runs every case in its own process, which is what makes the
// split work. See IonReleaseCountSurvivesARestart for the same root cause.

TEST_F(Helios, SurfaceModeTakesTheSurfaceBranch) {
    // The branch turns on sphere-map texture generation around the mesh
    // (helios.cpp:636-659), which is observable no matter where the emitters
    // have drifted to. Asserting on glDrawElements instead would depend on
    // whether the isosurface happens to form this frame, which is exactly the
    // history dependence described above.
    stop();
    dSurface = 0;
    start();
    draw();
    EXPECT_EQ(glstub::trace().countEnables(GL_TEXTURE_GEN_S), 0);
    EXPECT_EQ(glstub::trace().countCalls("glDrawElements"), 0);

    stop();
    dSurface = 1;
    start();
    draw();
    EXPECT_EQ(glstub::trace().countEnables(GL_TEXTURE_GEN_S), 1);
    EXPECT_EQ(glstub::trace().countEnables(GL_TEXTURE_GEN_T), 1);
    EXPECT_TRUE(savertest::NoEnableStateLeaked())
        << "the branch must switch texture generation back off";
}

// There is deliberately no test that the mesh itself comes out, even though
// impSurface draws it through glDrawElements and that path is worth covering.
// helios can only build a surface once per process:
//
//   - surfaceFunction caches its sphere count in a function-local static
//     (helios.cpp:440) while doSaver sizes the spheres array from the current
//     settings (helios.cpp:857). Restarting with fewer emitters leaves it
//     summing spheres past the end of the array, on every sample of a 70x70x70
//     volume. A test written to demonstrate that access-violates rather than
//     failing an assertion - which is how it was confirmed, and why it is
//     recorded in docs/MAINTENANCE.md instead of pinned here.
//   - even at unchanged settings a second cycle produces no isosurface, so the
//     cached count is not the only thing draw() carries across a restart.
//
// microcosm drives the same impSurface vertex-array path, so asserting only on
// the branch here loses no coverage of the library.

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

TEST_F(Helios, IonReleaseCountSurvivesARestart) {
    // DEFECT, pinned rather than fixed.
    //
    // ionsReleased is a function-local static inside draw() (helios.cpp:452),
    // so it is never reset. doSaver reallocates ilist to the current dIons but
    // leaves the counter where it was, and the draw loop still runs to it
    // (helios.cpp:629).
    //
    // Restarting with a SMALLER dIons therefore indexes ilist past its end.
    // This test does not do that - an out-of-bounds read is undefined and a
    // deliberate one does not belong in CI - it pins the cause instead: after a
    // restart the counter is still above zero, where a fresh saver would draw
    // nothing on its first frame.
    //
    // Harmless in the shipped saver, where settings only change between runs.
    // Recorded in docs/MAINTENANCE.md; fixing it means resetting the counter in
    // doSaver, which is a saver change rather than a test change.
    start();
    for (int frame = 0; frame < 10; ++frame) draw();

    stop();
    dIons = 200;  // larger, so the stale counter stays in bounds
    start();
    glstub::reset();
    draw();

    EXPECT_GT(glstub::trace().countCalls("glCallList"), 0)
        << "if this now fails the counter is being reset and the defect is gone";
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

TEST(HeliosFramework, ReadRegistryLeavesEveryValueUsable) {
    // Read-only: setDefaults runs first and the function returns early if the
    // key is absent, so this cannot disturb the machine. That early return also
    // means it covers little where the saver has never stored settings, CI
    // included - see the note in test_cyclone.cpp.
    setDefaults();
    readRegistry();

    EXPECT_GT(dIons, 0) << "the ion array is allocated with this count";
    EXPECT_GT(dEmitters, 0) << "so is the emitter array";
    EXPECT_GT(dAttracters, 0);
    EXPECT_GT(dSpeed, 0);
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
