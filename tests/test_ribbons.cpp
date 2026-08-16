/*
 * Tests for the ribbons saver.
 *
 * ribbons is the first saver written against the current entry-point contract
 * from the start: it carries a windows.h-free settings header
 * (ribbonsSettings.h, tested separately in test_ribbonsSettings.cpp), clamps
 * every registry value where it reads it, and draws from rsMath's generator
 * rather than a private copy of it - so this suite can seed the dice. Shared
 * scaffolding lives in support/saver_test_common.h.
 *
 * The one thing to know before reading further: a ribbon draws nothing until
 * its trail holds four samples, and samples are only pushed by update(), which
 * is driven by frameTime. A loop of bare draw() calls therefore renders an
 * empty frame forever. Every test that expects geometry runs frames() below.
 */

#include "support/saver_test_common.h"

#include <rsMath/rsMath.h>

#include "resource.h"
#include "ribbonsSettings.h"

// ribbons.cpp has no header; its contract with the framework is by name.
// SonarCloud cpp:S5421 flags these as mutable globals; they are declarations of
// the saver's own, which is Task 6 in docs/MAINTENANCE.md.
extern int dRibbonCount;
extern int dRibbonLength;
extern int dRibbonWidth;
extern int dSpeed;
extern int dColorCycling;
extern int dTransparency;
extern int readyToDraw;
extern float frameTime;

void setDefaults(int which);
void readRegistry();
void initControls(HWND hdlg);
LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

namespace {

// Each ribbon is drawn as this many overlapping triangle strips, one per glow
// pass (ribbons.cpp, NUM_PASSES). Asserted rather than assumed so that changing
// the pass count fails here rather than silently rewriting what "one ribbon"
// means in the tests below.
constexpr int kPassesPerRibbon = 12;

class Ribbons : public savertest::SaverFixture {
protected:
    void SetUp() override {
        rsRandGen().seed(savertest::kTestSeed);
        setDefaults(DEFAULTS1);
        frameTime = 0.0f;
    }

    void TearDown() override {
        frameTime = 0.0f;
        savertest::SaverFixture::TearDown();
    }

    // Run n frames of real time. Without this the trail never fills and draw()
    // returns before emitting a single vertex.
    void frames(int n) {
        frameTime = 1.0f / 60.0f;
        for (int i = 0; i < n; i++) {
            draw();
        }
    }

    // Enough frames for the trail to pass the four-sample threshold at any
    // speed setting: the slowest sampling rate is about one sample per frame.
    void framesUntilDrawing() { frames(20); }
};

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(RibbonsHarness, SaverBodyWasActuallyCompiled) {
    // Guards the WIN32-define trap: without it the saver preprocesses away and
    // every other test passes against an empty translation unit.
    setDefaults(DEFAULTS1);
    EXPECT_EQ(dRibbonCount, ribbons::kGentle.ribbonCount);
    EXPECT_EQ(dRibbonLength, ribbons::kGentle.ribbonLength);
    EXPECT_EQ(dSpeed, ribbons::kGentle.speed);
}

TEST(RibbonsHarness, EveryPresetSitsInsideTheDeclaredRanges) {
    // The header declares the ranges and the presets; the saver picks which one
    // to apply. Nothing else checks that all three agree with the bounds.
    const int presets[] = { DEFAULTS1, DEFAULTS2, DEFAULTS3 };
    for (int which : presets) {
        setDefaults(which);
        EXPECT_GE(dRibbonCount, ribbons::kRibbonCount.lo) << "preset " << which;
        EXPECT_LE(dRibbonCount, ribbons::kRibbonCount.hi) << "preset " << which;
        EXPECT_GE(dRibbonLength, ribbons::kRibbonLength.lo) << "preset " << which;
        EXPECT_LE(dRibbonLength, ribbons::kRibbonLength.hi) << "preset " << which;
        EXPECT_GE(dRibbonWidth, ribbons::kRibbonWidth.lo) << "preset " << which;
        EXPECT_LE(dRibbonWidth, ribbons::kRibbonWidth.hi) << "preset " << which;
        EXPECT_GE(dSpeed, ribbons::kSpeed.lo) << "preset " << which;
        EXPECT_LE(dSpeed, ribbons::kSpeed.hi) << "preset " << which;
        EXPECT_GE(dColorCycling, ribbons::kColorCycling.lo) << "preset " << which;
        EXPECT_LE(dColorCycling, ribbons::kColorCycling.hi) << "preset " << which;
        EXPECT_GE(dTransparency, ribbons::kTransparency.lo) << "preset " << which;
        EXPECT_LE(dTransparency, ribbons::kTransparency.hi) << "preset " << which;
    }
}

// --- a frame ---------------------------------------------------------------

TEST_F(Ribbons, FrameLeavesTheMatrixStackBalanced) {
    start();
    framesUntilDrawing();
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

TEST_F(Ribbons, FramePairsBeginAndEnd) {
    start();
    framesUntilDrawing();
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

TEST_F(Ribbons, PrimitiveVertexCountsAreLegal) {
    start();
    framesUntilDrawing();
    EXPECT_TRUE(savertest::VertexCountsLegal());
}

TEST_F(Ribbons, DoesNotLeakEnableState) {
    start();
    framesUntilDrawing();
    EXPECT_TRUE(savertest::NoEnableStateLeaked());
}

TEST_F(Ribbons, MakesNoInvalidReadbacks) {
    start();
    framesUntilDrawing();
    EXPECT_TRUE(savertest::NoInvalidEnums());
}

TEST_F(Ribbons, DrawsNothingUntilTheTrailHasFilled) {
    // Pins the frameTime trap in place: with no elapsed time no sample is ever
    // pushed, so a saver that looks busy draws nothing at all.
    start();
    frameTime = 0.0f;
    draw();
    EXPECT_EQ(glstub::trace().totalVertices(), 0u)
        << "a zero-length frame should advance nothing";
}

TEST_F(Ribbons, DrawsEveryRibbonAsOneStripPerPass) {
    dRibbonCount = 3;
    start();
    framesUntilDrawing();

    glstub::reset();
    frames(1);              // exactly one frame's worth of geometry
    EXPECT_EQ(countPrimitives(GL_TRIANGLE_STRIP), dRibbonCount * kPassesPerRibbon);
}

TEST_F(Ribbons, MoreRibbonsMeansMoreVertices) {
    dRibbonCount = 2;
    start();
    framesUntilDrawing();
    glstub::reset();
    frames(1);
    const unsigned long long few = glstub::trace().totalVertices();

    stop();                 // change counts only while nothing is allocated
    dRibbonCount = 6;
    start();
    framesUntilDrawing();
    glstub::reset();
    frames(1);
    const unsigned long long many = glstub::trace().totalVertices();

    EXPECT_GT(few, 0u) << "the trail should have filled by now";
    EXPECT_GT(many, few);
}

// --- restarting ------------------------------------------------------------

TEST_F(Ribbons, SurvivesBeingStoppedAndStartedAgain) {
    // None of the older savers was written to be restarted in a single process,
    // and six of the nine defects the harness has found were that bug wearing
    // different hats. ribbons is new code, so it is expected to hold: cleanUp
    // frees the ribbon array and the text writer, and owns nulling both.
    start();
    framesUntilDrawing();
    stop();

    start();
    framesUntilDrawing();

    EXPECT_GT(glstub::trace().totalVertices(), 0u);
    EXPECT_TRUE(savertest::PrimitivesPaired());
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

// --- framework entry points ------------------------------------------------

TEST_F(Ribbons, IdleProcSkipsDrawingWhenNotReady) {
    start();
    framesUntilDrawing();
    readyToDraw = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().totalVertices(), 0u);
    readyToDraw = 1;
}

TEST(RibbonsFramework, ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy) {
    setDefaults(DEFAULTS1);
    readyToDraw = 0;

    screenSaverProc(testsupport::hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(readyToDraw, 1);

    screenSaverProc(testsupport::hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(readyToDraw, 0);
}

TEST(RibbonsFramework, ReadRegistryLeavesEveryValueInRange) {
    // Holds whether or not a registry key exists: with no key readRegistry
    // returns on the defaults it just applied, and with one every value is
    // clamped where it is read. This is what Task 11 should make true
    // everywhere.
    dRibbonCount = -1;
    dRibbonLength = 100000;
    dRibbonWidth = -50;
    dSpeed = 100000;
    dColorCycling = -1;
    dTransparency = 100000;

    readRegistry();

    EXPECT_GE(dRibbonCount, ribbons::kRibbonCount.lo);
    EXPECT_LE(dRibbonCount, ribbons::kRibbonCount.hi);
    EXPECT_GE(dRibbonLength, ribbons::kRibbonLength.lo);
    EXPECT_LE(dRibbonLength, ribbons::kRibbonLength.hi);
    EXPECT_GE(dRibbonWidth, ribbons::kRibbonWidth.lo);
    EXPECT_LE(dRibbonWidth, ribbons::kRibbonWidth.hi);
    EXPECT_GE(dSpeed, ribbons::kSpeed.lo);
    EXPECT_LE(dSpeed, ribbons::kSpeed.hi);
    EXPECT_GE(dColorCycling, ribbons::kColorCycling.lo);
    EXPECT_LE(dColorCycling, ribbons::kColorCycling.hi);
    EXPECT_GE(dTransparency, ribbons::kTransparency.lo);
    EXPECT_LE(dTransparency, ribbons::kTransparency.hi);
}

// --- dialog procedures -----------------------------------------------------

TEST(RibbonsDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_TRUE(savertest::AboutProcColoursTheWebPageLabel(aboutProc));
}

TEST(RibbonsDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(aboutProc));
}

TEST(RibbonsDialogs, InitControlsRunsWithoutADialog) {
    setDefaults(DEFAULTS1);
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(RibbonsDialogs, ConfigureDialogHandlesTheStandardMessages) {
    EXPECT_TRUE(savertest::ConfigureDialogInitialisesAndCancels(screenSaverConfigureDialog));
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(screenSaverConfigureDialog));
}

TEST(RibbonsDialogs, ConfigureDialogAppliesEachPreset) {
    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS2, 0);
    EXPECT_EQ(dRibbonCount, ribbons::kVivid.ribbonCount);
    EXPECT_EQ(dSpeed, ribbons::kVivid.speed);

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS3, 0);
    EXPECT_EQ(dRibbonCount, ribbons::kChaos.ribbonCount);
    EXPECT_EQ(dSpeed, ribbons::kChaos.speed);

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS1, 0);
    EXPECT_EQ(dRibbonCount, ribbons::kGentle.ribbonCount);
    EXPECT_EQ(dSpeed, ribbons::kGentle.speed);
}
