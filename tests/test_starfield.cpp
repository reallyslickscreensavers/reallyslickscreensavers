/*
 * Tests for the starfield saver.
 *
 * starfield is the only saver that already has a windows.h-free settings header
 * (starfieldSettings.h, tested separately in test_starfieldSettings.cpp), so it
 * is the one place clamping can be checked against the saver's own readRegistry
 * rather than only in isolation. Shared scaffolding lives in
 * support/saver_test_common.h.
 */

#include "support/saver_test_common.h"


#include "resource.h"
#include "starfieldSettings.h"

// starfield.cpp has no header; its contract with the framework is by name.
// SonarCloud cpp:S5421 flags these as mutable globals; they are declarations of
// the saver's own, which is Task 6 in docs/MAINTENANCE.md.
extern int dNumStars;
extern int dSpeed;
extern int dStarSize;
extern int readyToDraw;

void setDefaults();
void readRegistry();
void initControls(HWND hdlg);
LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

namespace {

class Starfield : public savertest::SaverFixture {
protected:
    void SetUp() override { setDefaults(); }
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
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

TEST_F(Starfield, FramePairsBeginAndEnd) {
    start();
    draw();
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

TEST_F(Starfield, PrimitiveVertexCountsAreLegal) {
    start();
    draw();
    EXPECT_TRUE(savertest::VertexCountsLegal());
}

TEST_F(Starfield, DoesNotLeakEnableState) {
    start();
    draw();
    EXPECT_TRUE(savertest::NoEnableStateLeaked());
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
    EXPECT_EQ(glstub::trace().totalVertices(), 150u);

    stop();                 // change counts only while nothing is allocated
    dNumStars = 1500;
    start();
    draw();
    EXPECT_EQ(glstub::trace().totalVertices(), 1500u);
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

    screenSaverProc(testsupport::hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(readyToDraw, 1);

    screenSaverProc(testsupport::hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(readyToDraw, 0);
}

TEST(StarfieldFramework, ReadRegistryClampsEveryValueIntoRange) {
    // starfield is the one saver whose readRegistry runs every value through
    // clampToRange, so this holds whether or not a registry key exists - unlike
    // the equivalent tests in the other suites. This is what Task 11 should
    // make true everywhere.
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

TEST(StarfieldDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_TRUE(savertest::AboutProcColoursTheWebPageLabel(aboutProc));
}

TEST(StarfieldDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(aboutProc));
}

TEST(StarfieldDialogs, InitControlsRunsWithoutADialog) {
    setDefaults();
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(StarfieldDialogs, ConfigureDialogHandlesTheStandardMessages) {
    EXPECT_TRUE(savertest::ConfigureDialogInitialisesAndCancels(screenSaverConfigureDialog));
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(screenSaverConfigureDialog));
}

TEST(StarfieldDialogs, ConfigureDialogRestoresDefaults) {
    setDefaults();
    const int defaultStars = dNumStars;
    dNumStars = 7;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS, 0);

    EXPECT_EQ(dNumStars, defaultStars);
}
