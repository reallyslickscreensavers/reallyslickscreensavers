/*
 * Tests for the cyclone saver.
 *
 * cyclone carries the two BLOCKER findings SonarCloud reports against this
 * repository (cpp:S3519 at cyclone.cpp:155 and :163), so beyond coverage these
 * tests exist to pin the invariant that makes those accesses safe. Shared
 * scaffolding lives in support/saver_test_common.h.
 */

#include "support/saver_test_common.h"


#include "resource.h"

// cyclone.cpp has no header; its contract with the framework is by name.
// SonarCloud cpp:S5421 flags these as mutable globals; they are declarations of
// the saver's own, which is Task 6 in docs/MAINTENANCE.md.
extern int dCyclones;
extern int dParticles;
extern int dSize;
extern int dComplexity;
extern BOOL dShowCurves;
extern int readyToDraw;

void setDefaults();
void readRegistry();
void initControls(HWND hdlg);
LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

namespace {

class Cyclone : public savertest::SaverFixture {
protected:
    void SetUp() override {
        rsRandGen().seed(savertest::kTestSeed);
        setDefaults();
    }
};

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(CycloneHarness, SaverBodyWasActuallyCompiled) {
    setDefaults();
    EXPECT_EQ(dCyclones, 1);
    EXPECT_EQ(dParticles, 400);
    EXPECT_EQ(dComplexity, 3);
    EXPECT_EQ(dSize, 7);
}

// --- the BLOCKER guard -----------------------------------------------------
//
// SonarCloud reports cpp:S3519 at cyclone.cpp:155 and :163: a heap access at a
// negative byte offset, and an index past the end. Both index xyz[], which
// initSaver allocates as new float*[dComplexity+3] and then walks down from
// dComplexity+2. Reaching them needs a negative dComplexity.
//
// That cannot happen: screenSaverProc calls readRegistry() before initSaver(),
// and readRegistry clamps dComplexity to 1..10 unconditionally - the clamp sits
// outside the RegQueryValueEx success check, so it applies to the default value
// too (cyclone.cpp:646, added by PR #35).
//
// KNOWN LIMITATION - read this before trusting the guard.
//
// readRegistry returns early when HKCU\Software\Really Slick\Cyclone does not
// exist, which is the case on a fresh CI runner and on any machine where the
// saver has never stored settings. There the clamp lines never execute and
// these tests only confirm that setDefaults' value survives. They bite fully
// only where a key exists.
//
// Exercising the populated path means writing to that real key, which would
// modify the developer's own saver settings, so it is deliberately not done.
// The way to make this guard unconditional is to give cyclone a settings header
// with a pure clamp function - the starfieldSettings.h / rsWin32SaverSettings.h
// pattern - and test that directly. That is Task 11's refactor.
//
// The same caveat applies to the ReadRegistry tests in the other suites, and it
// is why coverage on CI sits about five points below a developer machine that
// has run the savers.

TEST(CycloneBlockerGuard, ReadRegistryAlwaysLeavesComplexityInRange) {
    dComplexity = -5;
    readRegistry();

    EXPECT_GE(dComplexity, 1) << "a negative complexity is what makes cyclone.cpp:155 reachable";
    EXPECT_LE(dComplexity, 10);
}

TEST(CycloneBlockerGuard, ComplexityStaysInRangeAcrossRepeatedReads) {
    for (int i = 0; i < 5; ++i) {
        dComplexity = (i % 2) ? -100 : 100000;
        readRegistry();
        ASSERT_GE(dComplexity, 1);
        ASSERT_LE(dComplexity, 10);
    }
}

TEST(CycloneBlockerGuard, CreateClampsBeforeAllocating) {
    // The ordering is the whole guarantee: WM_CREATE must read (and clamp)
    // before it allocates. Corrupt the value first; a correct handler overwrites
    // it via readRegistry before initSaver sizes anything from it.
    dComplexity = -42;
    readyToDraw = 0;

    screenSaverProc(testsupport::hostWindow(), WM_CREATE, 0, 0);

    EXPECT_GE(dComplexity, 1) << "initSaver sized its arrays from an unclamped value";
    EXPECT_EQ(readyToDraw, 1);

    screenSaverProc(testsupport::hostWindow(), WM_DESTROY, 0, 0);
}

// --- a frame ---------------------------------------------------------------

TEST_F(Cyclone, FrameLeavesTheMatrixStackBalanced) {
    start();
    draw();
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

TEST_F(Cyclone, FramePairsBeginAndEnd) {
    start();
    draw();
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

TEST_F(Cyclone, PrimitiveVertexCountsAreLegal) {
    start();
    draw();
    EXPECT_TRUE(savertest::VertexCountsLegal());
}

TEST_F(Cyclone, DoesNotLeakEnableState) {
    start();
    draw();
    EXPECT_TRUE(savertest::NoEnableStateLeaked());
}

TEST_F(Cyclone, FrameDrawsEveryParticleFromTheCompiledBlob) {
    // cyclone's particles are a display list called once per particle, so a
    // frame emits no immediate-mode vertices at all. Counting glCallList is the
    // only way to see the geometry.
    dCyclones = 2;
    dParticles = 25;
    start();
    draw();

    EXPECT_EQ(glstub::trace().countCalls("glCallList"), dCyclones * dParticles);
    EXPECT_EQ(glstub::trace().totalVertices(), 0u)
        << "particles come from a display list; immediate-mode vertices would be a redesign";
}

// --- settings change what is drawn -----------------------------------------

TEST_F(Cyclone, ShowCurvesAddsLineStrips) {
    dShowCurves = FALSE;
    start();
    draw();
    const int without = countPrimitives(GL_LINE_STRIP);

    dShowCurves = TRUE;
    glstub::reset();
    draw();
    const int with = countPrimitives(GL_LINE_STRIP);

    EXPECT_GT(with, without) << "the curve overlay draws the spline as line strips";
}

TEST_F(Cyclone, CurveOverlayDrawsAFixedNumberOfSamples) {
    // The float loop counter this replaced accumulated 0.02f in single
    // precision and stood at about 0.9999996 after fifty additions, so it ran
    // a fifty-first time and drew a duplicate vertex. cpp:S2193, Task 10 in
    // docs/MAINTENANCE.md.
    //
    // cyclone::update() runs once per cyclone per frame (cyclone.cpp:477-478),
    // the particles are display lists that emit no immediate-mode vertices,
    // and textwriter->draw is behind kStatistics (0 in the test shim), so with
    // one cyclone these two strips are the only line strips in the frame.
    dShowCurves = TRUE;
    dCyclones = 1;
    dParticles = 4;
    start();
    draw();

    std::vector<glstub::Primitive> strips;
    for (const auto& p : glstub::trace().primitives) {
        if (p.mode == GL_LINE_STRIP) strips.push_back(p);
    }

    ASSERT_EQ(strips.size(), 2u);
    EXPECT_EQ(strips[0].vertices, 50u) << "the sampled curve";
    EXPECT_EQ(strips[1].vertices, static_cast<unsigned>(dComplexity + 3)) << "the control polygon";
}

TEST_F(Cyclone, MoreCyclonesMeansMoreDrawing) {
    dParticles = 20;
    dCyclones = 1;
    start();
    draw();
    const int one = glstub::trace().countCalls("glCallList");

    stop();                 // change counts only while nothing is allocated
    dCyclones = 3;
    start();
    draw();
    const int three = glstub::trace().countCalls("glCallList");

    EXPECT_GT(one, 0);
    EXPECT_GT(three, one) << "each cyclone contributes its own particles";
}

TEST_F(Cyclone, IdleProcSkipsDrawingWhenNotReady) {
    start();
    readyToDraw = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().totalVertices(), 0u);
    readyToDraw = 1;
}

// --- dialog procedures -----------------------------------------------------

TEST(CycloneDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_TRUE(savertest::AboutProcColoursTheWebPageLabel(aboutProc));
}

TEST(CycloneDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(aboutProc));
}

TEST(CycloneDialogs, InitControlsRunsWithoutADialog) {
    setDefaults();
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(CycloneDialogs, ConfigureDialogHandlesTheStandardMessages) {
    EXPECT_TRUE(savertest::ConfigureDialogInitialisesAndCancels(screenSaverConfigureDialog));
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(screenSaverConfigureDialog));
}

TEST(CycloneDialogs, ConfigureDialogRestoresDefaults) {
    setDefaults();
    const int defaultParticles = dParticles;
    dParticles = 3;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS, 0);

    EXPECT_EQ(dParticles, defaultParticles);
}
