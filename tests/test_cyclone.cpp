/*
 * Tests for the cyclone saver.
 *
 * cyclone carries the two BLOCKER findings SonarCloud reports against this
 * repository (cpp:S3519 at cyclone.cpp:155 and :163), so beyond coverage these
 * tests exist to pin the invariant that makes those accesses safe.
 */

#include <gtest/gtest.h>

#include <Windows.h>
#include <gl/GL.h>

#include "support/gl_stub.h"
#include "support/test_window.h"
#include "resource.h"

// cyclone.cpp has no header; its contract with the framework is by name.
//
// The savers keep their settings in mutable globals (docs/MAINTENANCE.md
// Task 6). These accessors reach them without this file declaring globals of
// its own: the extern declarations are block-scope, so they bind to the
// definitions in cyclone.cpp. They must sit at global scope - inside a
// namespace a block-scope extern would bind to that namespace instead.
static int&  svCyclones()    { extern int dCyclones;    return dCyclones; }
static int&  svParticles()   { extern int dParticles;   return dParticles; }
static int&  svSize()        { extern int dSize;        return dSize; }
static int&  svComplexity()  { extern int dComplexity;  return dComplexity; }
static BOOL& svShowCurves()  { extern BOOL dShowCurves; return dShowCurves; }
static int&  svReadyToDraw() { extern int readyToDraw;  return readyToDraw; }

void setDefaults();
void draw();
void idleProc();
void initSaver(HWND hwnd);
void cleanUp(HWND hwnd);
void readRegistry();
void initControls(HWND hdlg);
LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

namespace {

HWND hostWindow() { return testsupport::hostWindow(); }

int countPrimitives(unsigned mode) {
    int n = 0;
    for (const auto& p : glstub::trace().primitives) if (p.mode == mode) n++;
    return n;
}

// initSaver allocates per-cyclone arrays sized from dComplexity, so pair it
// with cleanUp.
class Cyclone : public ::testing::Test {
protected:
    void SetUp() override { setDefaults(); }
    void TearDown() override { if (started_) cleanUp(hostWindow()); }
    // Throws the first frame away so every test measures a warm one. ctest runs
    // each case in its own process, so anything a cold frame does once - lazy
    // resource construction, first-call statics - would otherwise make a test
    // pass in suite order and fail in isolation.
    void start() {
        initSaver(hostWindow());
        started_ = true;
        draw();           // warm-up
        glstub::reset();
    }

    void restart() {
        cleanUp(hostWindow());
        started_ = false;
        start();
    }
private:
    bool started_ = false;
};

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(CycloneHarness, SaverBodyWasActuallyCompiled) {
    setDefaults();
    EXPECT_EQ(svCyclones(), 1);
    EXPECT_EQ(svParticles(), 400);
    EXPECT_EQ(svComplexity(), 3);
    EXPECT_EQ(svSize(), 7);
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
// These tests pin that reasoning. If someone removes the clamp, the findings
// stop being theoretical and this fails.
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
// The same caveat applies to the ReadRegistry tests in the plasma and flocks
// suites, and it is why coverage on CI sits about 5 points below a developer
// machine that has run the savers.

TEST(CycloneBlockerGuard, ReadRegistryAlwaysLeavesComplexityInRange) {
    // Whatever is stored on this machine - or if the key is absent entirely -
    // readRegistry must leave dComplexity within the range initSaver allocates
    // for. Read-only: setDefaults runs first and it returns early on a missing key.
    svComplexity() = -5;
    readRegistry();

    EXPECT_GE(svComplexity(), 1) << "a negative complexity is what makes cyclone.cpp:155 reachable";
    EXPECT_LE(svComplexity(), 10);
}

TEST(CycloneBlockerGuard, ComplexityStaysInRangeAcrossRepeatedReads) {
    for (int i = 0; i < 5; ++i) {
        svComplexity() = (i % 2) ? -100 : 100000;
        readRegistry();
        ASSERT_GE(svComplexity(), 1);
        ASSERT_LE(svComplexity(), 10);
    }
}

TEST(CycloneBlockerGuard, CreateClampsBeforeAllocating) {
    // The ordering is the whole guarantee: WM_CREATE must read (and clamp)
    // before it allocates. Corrupt the value first; a correct handler overwrites
    // it via readRegistry before initSaver sizes anything from it.
    svComplexity() = -42;
    svReadyToDraw() = 0;

    screenSaverProc(hostWindow(), WM_CREATE, 0, 0);

    EXPECT_GE(svComplexity(), 1) << "initSaver sized its arrays from an unclamped value";
    EXPECT_EQ(svReadyToDraw(), 1);

    screenSaverProc(hostWindow(), WM_DESTROY, 0, 0);
}

// --- a frame ---------------------------------------------------------------

TEST_F(Cyclone, FrameLeavesTheMatrixStackBalanced) {
    start();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_TRUE(t.matrixBalanced())
        << "depth " << t.matrixDepth << ", " << t.pushes << " pushes vs " << t.pops << " pops";
    EXPECT_GE(t.minMatrixDepth, 0);
}

TEST_F(Cyclone, FramePairsBeginAndEnd) {
    start();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_TRUE(t.primitivesBalanced()) << t.begins << " glBegin vs " << t.ends << " glEnd";
    EXPECT_FALSE(t.nestedBeginSeen);
    EXPECT_FALSE(t.vertexOutsideBegin);
}

TEST_F(Cyclone, PrimitiveVertexCountsAreLegal) {
    start();
    draw();

    std::string why;
    EXPECT_TRUE(glstub::primitiveVertexCountsLegal(&why)) << why;
}

TEST_F(Cyclone, FrameDrawsEveryParticleFromTheCompiledBlob) {
    // cyclone's particles are a display list called once per particle, so a
    // frame emits no immediate-mode vertices at all. Counting glCallList is the
    // only way to see the geometry.
    svCyclones() = 2;
    svParticles() = 25;
    start();
    draw();

    EXPECT_EQ(glstub::trace().countCalls("glCallList"), svCyclones() * svParticles());
    EXPECT_EQ(glstub::trace().totalVertices(), 0u)
        << "particles come from a display list; immediate-mode vertices would be a redesign";
}

TEST_F(Cyclone, DoesNotLeakEnableState) {
    start();
    draw();
    for (const auto& [capability, net] : glstub::trace().enables) {
        EXPECT_EQ(net, 0) << "capability " << capability << " left with net enable " << net;
    }
}

// --- settings change what is drawn -----------------------------------------

TEST_F(Cyclone, ShowCurvesAddsLineStrips) {
    svShowCurves() = FALSE;
    start();
    draw();
    const int without = countPrimitives(GL_LINE_STRIP);

    svShowCurves() = TRUE;
    glstub::reset();
    draw();
    const int with = countPrimitives(GL_LINE_STRIP);

    EXPECT_GT(with, without) << "the curve overlay draws the spline as line strips";
}

TEST_F(Cyclone, MoreCyclonesMeansMoreDrawing) {
    svParticles() = 20;
    svCyclones() = 1;
    start();
    draw();
    const int one = glstub::trace().countCalls("glCallList");

    svCyclones() = 3;
    restart();
    draw();
    const int three = glstub::trace().countCalls("glCallList");

    EXPECT_GT(one, 0);
    EXPECT_GT(three, one) << "each cyclone contributes its own particles";
}

TEST_F(Cyclone, IdleProcSkipsDrawingWhenNotReady) {
    start();
    svReadyToDraw() = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().totalVertices(), 0u);
    svReadyToDraw() = 1;
}

// --- dialog procedures -----------------------------------------------------
//
// IDOK is never sent: it calls writeRegistry and would rewrite real settings.

TEST(CycloneDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_NE(aboutProc(nullptr, WM_CTLCOLORSTATIC, 0, 0), 0);
}

TEST(CycloneDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_EQ(aboutProc(nullptr, WM_MOUSEMOVE, 0, 0), FALSE);
}

TEST(CycloneDialogs, InitControlsRunsWithoutADialog) {
    setDefaults();
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(CycloneDialogs, ConfigureDialogInitialisesAndCancels) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_INITDIALOG, 0, 0), TRUE);
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_COMMAND, IDCANCEL, 0), TRUE);
}

TEST(CycloneDialogs, ConfigureDialogRestoresDefaults) {
    setDefaults();
    const int defaultParticles = svParticles();
    svParticles() = 3;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS, 0);

    EXPECT_EQ(svParticles(), defaultParticles);
}

TEST(CycloneDialogs, ConfigureDialogHandlesSliderMovement) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_HSCROLL, 0, 0), TRUE);
}

TEST(CycloneDialogs, ConfigureDialogIgnoresUnknownMessages) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_MOUSEMOVE, 0, 0), FALSE);
}
