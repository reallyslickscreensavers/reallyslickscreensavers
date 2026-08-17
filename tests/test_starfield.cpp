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

#include <limits>
#include <vector>

#include "resource.h"
#include "starfieldSettings.h"

// starfield.cpp has no header; its contract with the framework is by name.
// SonarCloud cpp:S5421 flags these as mutable globals; they are declarations of
// the saver's own, which is Task 6 in docs/MAINTENANCE.md.
extern int dNumStars;
extern int dSpeed;
extern int dStarSize;
extern int readyToDraw;
extern float frameTime;
extern std::vector<float> starZ;
extern std::vector<float> starV;

void setDefaults();
void readRegistry();
void initControls(HWND hdlg);
LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

namespace {

class Starfield : public savertest::SaverFixture {
protected:
    void SetUp() override {
        rsRandGen().seed(savertest::kTestSeed);
        setDefaults();
    }
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

// Added in step 3, before the NaN-guard rewrite (Task 12 in
// docs/MAINTENANCE.md), and expected to pass both before and after it: this is
// not new defect coverage, starfield.cpp already clamps bucket > maxStarSize.
// It exists so the rewrite cannot silently drop that clamp unnoticed.
// readRegistry clamps dStarSize, so this is a backstop on a raw array index
// rather than the primary bound.
TEST_F(Starfield, AnOversizedStarSizeStaysInsideTheSizeBuckets) {
    dStarSize = 10000;      // stop-change-start: set before start()
    start();
    draw();

    EXPECT_EQ(glstub::trace().totalVertices(), static_cast<unsigned long long>(dNumStars));
    EXPECT_LE(countPrimitives(GL_POINTS), starfield::kStarSize.hi);
}

// The step-4 guard (Task 12 in docs/MAINTENANCE.md). A NaN frameTime makes
// starZ, and therefore brightness and size, NaN for every star. Both the old
// `size < 1.0f` and `bucket > maxStarSize` tests are comparisons a NaN passes
// through, which is how a NaN size used to reach sizeBuckets[] as a raw,
// out-of-bounds index; the rewritten guard is an is-in-range test, so a NaN
// falls to bucket 1 like everything else that is not in range. Under the
// guard every star lands in that one bucket, so exactly one GL_POINTS batch
// is emitted.
TEST_F(Starfield, NanFrameTimeKeepsStarsInsideTheSizeBuckets) {
    start();
    frameTime = std::numeric_limits<float>::quiet_NaN();

    draw();

    EXPECT_EQ(countPrimitives(GL_POINTS), 1);
    EXPECT_EQ(glstub::trace().countCalls("glPointSize"), 1);
    EXPECT_EQ(glstub::trace().totalVertices(), static_cast<unsigned long long>(dNumStars));

    frameTime = 0.0f;
}

// The Task 12 tripwire (docs/MAINTENANCE.md): the only test that would notice
// a private rsRandi/rsRandf/rsRandGen copy returning to starfield.cpp. If the
// two runs below disagree, the fix is to equalise them - look for an
// asymmetry in frame count, in frameTime at either warm-up, or in generator
// state between the seed and the first start() - never to weaken the
// EXPECT_EQ below. Weakening it would silently discard the only regression
// tripwire the ODR fix has.
TEST_F(Starfield, StarLayoutRepeatsForTheSameSeed) {
    dNumStars = 50;
    dSpeed = starfield::kSpeed.hi;  // both set before any start(), stop-change-start

    rsRandGen().seed(savertest::kTestSeed);
    // Immediately before start(): start() draws a warm-up frame it then
    // discards, and draw() is the only consumer of frameTime while idleProc is
    // the only writer, so at zero that frame moves nothing - no star can fail
    // the respawn test at starfield.cpp:124-126, and no rsRandf is drawn. That
    // is what makes both runs below enter their 40-frame loops with identical
    // generator state.
    frameTime = 0.0f;
    start();
    const std::vector<float> initialZ = starZ;

    for (int f = 0; f < 40; ++f) {
        // frameTime must be set inside the loop, before each draw() - it is a
        // global that only idleProc otherwise writes, and at zero the frame
        // simulates nothing (see tests/test_skyrocket.cpp).
        frameTime = 1.0f;
        draw();
    }

    const std::vector<float> expectedZ = starZ;
    const std::vector<float> expectedV = starV;
    stop();

    rsRandGen().seed(savertest::kTestSeed);
    frameTime = 0.0f;
    start();

    for (int f = 0; f < 40; ++f) {
        frameTime = 1.0f;
        draw();
    }

    EXPECT_EQ(starZ, expectedZ);
    EXPECT_EQ(starV, expectedV);

    // Only the respawn block (starfield.cpp:124-131) raises starZ, via
    // farZ - rsRandf(10.0f), so counting stars whose final starZ exceeds their
    // initial entry proves that block ran, rather than assuming it. At
    // dSpeed == kSpeed.hi, baseSpeed == 50 and starV >= 0.15, so every star
    // loses at least 7.5 units of starZ per frame; 40 frames drives every one
    // of them through the respawn block at least once, by arithmetic rather
    // than luck.
    int respawned = 0;
    for (size_t i = 0; i < starZ.size(); ++i) {
        if (starZ[i] > initialZ[i]) ++respawned;
    }
    EXPECT_GT(respawned, 0);

    frameTime = 0.0f;
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
