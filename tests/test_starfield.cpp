/*
 * Tests for the starfield saver.
 *
 * starfield is the only saver that already has a windows.h-free settings header
 * (starfieldSettings.h, tested separately in test_starfieldSettings.cpp), so it
 * is the one place the clamping can be checked against the saver's own code
 * rather than only in isolation. This file covers the saver body itself.
 */

#include <gtest/gtest.h>

#include <Windows.h>
#include <gl/GL.h>

#include "support/gl_stub.h"
#include "support/test_window.h"
#include "resource.h"
#include "starfieldSettings.h"

// starfield.cpp has no header; its contract with the framework is by name.
//
// SonarCloud cpp:S5421 flags these as mutable globals. They are declarations,
// not definitions - the variables live in starfield.cpp - but the rule cannot
// tell the difference. See Task 6 in docs/MAINTENANCE.md.
extern int dNumStars;
extern int dSpeed;
extern int dStarSize;
extern int readyToDraw;

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

class Starfield : public ::testing::Test {
protected:
    void SetUp() override { setDefaults(); }
    void TearDown() override { if (started_) cleanUp(hostWindow()); }

    // Discards the first frame so every test measures a warm one; ctest runs
    // each case in its own process.
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

TEST(StarfieldHarness, SaverBodyWasActuallyCompiled) {
    // Guards the WIN32-define trap: without it the saver preprocesses away and
    // every other test passes against an empty translation unit.
    setDefaults();
    EXPECT_EQ(dNumStars, starfield::kDefaultNumStars);
    EXPECT_EQ(dSpeed, starfield::kDefaultSpeed);
    EXPECT_EQ(dStarSize, starfield::kDefaultStarSize);
}

TEST(StarfieldHarness, DefaultsSitInsideTheDeclaredRanges) {
    // The header declares the ranges and the saver picks the defaults; nothing
    // else checks that the two agree.
    setDefaults();
    EXPECT_GE(dNumStars, starfield::kNumStars.lo);
    EXPECT_LE(dNumStars, starfield::kNumStars.hi);
    EXPECT_GE(dSpeed, starfield::kSpeed.lo);
    EXPECT_LE(dSpeed, starfield::kSpeed.hi);
    EXPECT_GE(dStarSize, starfield::kStarSize.lo);
    EXPECT_LE(dStarSize, starfield::kStarSize.hi);
}

// --- a frame ---------------------------------------------------------------

TEST_F(Starfield, FrameLeavesTheMatrixStackBalanced) {
    start();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_TRUE(t.matrixBalanced())
        << "depth " << t.matrixDepth << ", " << t.pushes << " pushes vs " << t.pops << " pops";
    EXPECT_GE(t.minMatrixDepth, 0);
}

TEST_F(Starfield, FramePairsBeginAndEnd) {
    start();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_TRUE(t.primitivesBalanced()) << t.begins << " glBegin vs " << t.ends << " glEnd";
    EXPECT_FALSE(t.nestedBeginSeen);
    EXPECT_FALSE(t.vertexOutsideBegin);
}

TEST_F(Starfield, PrimitiveVertexCountsAreLegal) {
    start();
    draw();

    std::string why;
    EXPECT_TRUE(glstub::primitiveVertexCountsLegal(&why)) << why;
}

TEST_F(Starfield, DrawsEveryStarOncePerFrame) {
    // starfield buckets its stars by point size and opens one GL_POINTS block
    // per non-empty bucket, so the block count follows how many distinct sizes
    // happen to be on screen - not one, and not one per star. The invariant
    // that does hold is that every star is drawn exactly once.
    dNumStars = 200;
    start();
    draw();

    const int blocks = countPrimitives(GL_POINTS);
    EXPECT_GE(blocks, 1);
    EXPECT_LE(blocks, starfield::kStarSize.hi) << "at most one block per size bucket";
    EXPECT_EQ(glstub::trace().totalVertices(), static_cast<unsigned long long>(dNumStars))
        << "one vertex per star, however they are bucketed";
}

TEST_F(Starfield, MoreStarsMeansMoreVertices) {
    dNumStars = 150;
    start();
    draw();
    const unsigned long long few = glstub::trace().totalVertices();

    dNumStars = 1500;
    restart();
    draw();
    const unsigned long long many = glstub::trace().totalVertices();

    EXPECT_EQ(few, 150u);
    EXPECT_EQ(many, 1500u);
    EXPECT_GT(many, few);
}

TEST_F(Starfield, DoesNotLeakEnableState) {
    start();
    draw();
    for (const auto& [capability, net] : glstub::trace().enables) {
        EXPECT_EQ(net, 0) << "capability " << capability << " left with net enable " << net;
    }
}

// --- framework entry points ------------------------------------------------

TEST_F(Starfield, IdleProcSkipsDrawingWhenNotReady) {
    start();
    readyToDraw = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().totalVertices(), 0u);
    readyToDraw = 1;
}

TEST(StarfieldFramework, ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy) {
    setDefaults();
    readyToDraw = 0;

    screenSaverProc(hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(readyToDraw, 1);

    screenSaverProc(hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(readyToDraw, 0);
}

TEST(StarfieldFramework, ReadRegistryClampsEveryValueIntoRange) {
    // starfield is the one saver whose readRegistry runs every value through
    // clampToRange, so this holds whether or not a key exists - unlike the
    // equivalent tests in the other suites. This is what Task 11 should make
    // true everywhere.
    dNumStars = -1;
    dSpeed = 100000;
    dStarSize = -50;

    readRegistry();

    EXPECT_GE(dNumStars, starfield::kNumStars.lo);
    EXPECT_LE(dNumStars, starfield::kNumStars.hi);
    EXPECT_GE(dSpeed, starfield::kSpeed.lo);
    EXPECT_LE(dSpeed, starfield::kSpeed.hi);
    EXPECT_GE(dStarSize, starfield::kStarSize.lo);
    EXPECT_LE(dStarSize, starfield::kStarSize.hi);
}

// --- dialog procedures -----------------------------------------------------
//
// IDOK is never sent: it calls writeRegistry and would rewrite real settings.

TEST(StarfieldDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_NE(aboutProc(nullptr, WM_CTLCOLORSTATIC, 0, 0), 0);
}

TEST(StarfieldDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_EQ(aboutProc(nullptr, WM_MOUSEMOVE, 0, 0), FALSE);
}

TEST(StarfieldDialogs, InitControlsRunsWithoutADialog) {
    setDefaults();
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(StarfieldDialogs, ConfigureDialogInitialisesAndCancels) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_INITDIALOG, 0, 0), TRUE);
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_COMMAND, IDCANCEL, 0), TRUE);
}

TEST(StarfieldDialogs, ConfigureDialogRestoresDefaults) {
    setDefaults();
    const int defaultStars = dNumStars;
    dNumStars = 7;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS, 0);

    EXPECT_EQ(dNumStars, defaultStars);
}

TEST(StarfieldDialogs, ConfigureDialogHandlesSliderMovement) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_HSCROLL, 0, 0), TRUE);
}

TEST(StarfieldDialogs, ConfigureDialogIgnoresUnknownMessages) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_MOUSEMOVE, 0, 0), FALSE);
}
