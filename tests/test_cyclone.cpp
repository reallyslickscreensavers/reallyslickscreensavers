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
#include "cycloneSettings.h"

// cyclone.cpp has no header; its contract with the framework is by name.
// SonarCloud cpp:S5421 flags these as mutable globals; they are declarations of
// the saver's own, which is Task 6 in docs/MAINTENANCE.md.
extern int dCyclones;
extern int dParticles;
extern int dSize;
extern int dComplexity;
extern int dSpeed;
extern BOOL dStretch;
extern BOOL dShowCurves;
extern unsigned int dFrameRateLimit;
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

    void TearDown() override {
        glstub::setSwapControlAvailable(true);
        savertest::SaverFixture::TearDown();
    }
};

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(CycloneHarness, SaverBodyWasActuallyCompiled) {
    setDefaults();
    EXPECT_EQ(dCyclones, cycloneSettings::kDefaultCyclones);
    EXPECT_EQ(dParticles, cycloneSettings::kDefaultParticles);
    EXPECT_EQ(dComplexity, cycloneSettings::kDefaultComplexity);
    EXPECT_EQ(dSize, cycloneSettings::kDefaultSize);
    EXPECT_EQ(dSpeed, cycloneSettings::kDefaultSpeed);
    EXPECT_EQ(dStretch, TRUE);
    EXPECT_EQ(dShowCurves, FALSE);
    EXPECT_EQ(dFrameRateLimit, (unsigned int)cycloneSettings::kDefaultFrameRate);
}

TEST_F(Cyclone, DisablesVSyncForSoftwareFramePacing) {
    startCapturingSetup();

    ASSERT_EQ(glstub::trace().swapIntervals.size(), 1u);
    EXPECT_EQ(glstub::trace().swapIntervals.front(), 0);
}

TEST_F(Cyclone, StartsWithoutSwapControlSupport) {
    glstub::setSwapControlAvailable(false);

    startCapturingSetup();

    EXPECT_EQ(glstub::trace().countCalls("wglCreateContext"), 1);
    EXPECT_EQ(glstub::trace().countCalls("wglSwapIntervalEXT"), 0);
}

// --- settings contract -----------------------------------------------------

TEST(CycloneSettings, DefaultsLieWithinTheirRanges) {
    EXPECT_GE(cycloneSettings::kDefaultCyclones, cycloneSettings::kCyclones.lo);
    EXPECT_LE(cycloneSettings::kDefaultCyclones, cycloneSettings::kCyclones.hi);
    EXPECT_GE(cycloneSettings::kDefaultParticles, cycloneSettings::kParticles.lo);
    EXPECT_LE(cycloneSettings::kDefaultParticles, cycloneSettings::kParticles.hi);
    EXPECT_GE(cycloneSettings::kDefaultSize, cycloneSettings::kSize.lo);
    EXPECT_LE(cycloneSettings::kDefaultSize, cycloneSettings::kSize.hi);
    EXPECT_GE(cycloneSettings::kDefaultComplexity, cycloneSettings::kComplexity.lo);
    EXPECT_LE(cycloneSettings::kDefaultComplexity, cycloneSettings::kComplexity.hi);
    EXPECT_GE(cycloneSettings::kDefaultSpeed, cycloneSettings::kSpeed.lo);
    EXPECT_LE(cycloneSettings::kDefaultSpeed, cycloneSettings::kSpeed.hi);
    EXPECT_GE(cycloneSettings::kDefaultFrameRate, cycloneSettings::kFrameRate.lo);
    EXPECT_LE(cycloneSettings::kDefaultFrameRate, cycloneSettings::kFrameRate.hi);
}

TEST(CycloneSettings, RegistryValuesClampWithoutSignedNarrowing) {
    EXPECT_EQ(cycloneSettings::clampToRange(0, cycloneSettings::kCyclones), 1);
    EXPECT_EQ(cycloneSettings::clampToRange(5, cycloneSettings::kCyclones), 5);
    EXPECT_EQ(cycloneSettings::clampToRange(100, cycloneSettings::kCyclones), 10);
    EXPECT_EQ(cycloneSettings::clampToRange(0xFFFFFFFFUL, cycloneSettings::kParticles),
        cycloneSettings::kParticles.hi);
}

TEST(CycloneSettings, RegistryFlagsAreNormalized) {
    EXPECT_EQ(cycloneSettings::normalizeFlag(0), 0);
    EXPECT_EQ(cycloneSettings::normalizeFlag(1), 1);
    EXPECT_EQ(cycloneSettings::normalizeFlag(0xFFFFFFFFUL), 1);
}

TEST(CycloneSettings, UnlimitedFrameRateHasASensibleDisabledValue) {
    const cycloneSettings::FrameRateUi ui = cycloneSettings::frameRateToUi(0);
    EXPECT_FALSE(ui.limited);
    EXPECT_EQ(ui.fps, cycloneSettings::kDefaultFrameRate);
}

TEST(CycloneSettings, FrameRateRoundTripsThroughTheDialog) {
    const unsigned int values[] = { 0, 1, 30, 60, 144, 1000 };
    for(unsigned int value : values){
        const cycloneSettings::FrameRateUi ui = cycloneSettings::frameRateToUi(value);
        EXPECT_EQ(cycloneSettings::frameRateFromUi(ui.limited, ui.fps), value);
    }
}

TEST(CycloneSettings, CheckedFrameRateCannotSilentlyBecomeUnlimited) {
    EXPECT_EQ(cycloneSettings::frameRateFromUi(true, 0), 1u);
    EXPECT_EQ(cycloneSettings::frameRateFromUi(true, -1), 1u);
    EXPECT_EQ(cycloneSettings::frameRateFromUi(true, 5000), 1000u);
}

TEST(CycloneSettings, CorruptStoredFrameRateIsClamped) {
    const cycloneSettings::FrameRateUi ui =
        cycloneSettings::frameRateToUi(0xFFFFFFFFu);
    EXPECT_TRUE(ui.limited);
    EXPECT_EQ(ui.fps, cycloneSettings::kFrameRate.hi);
}

// cyclone needs no DefaultsSitInsideTheDeclaredRanges case of its own: it
// declares its defaults as kDefault* constants in cycloneSettings.h rather than
// as literals in setDefaults, so CycloneSettings.DefaultsLieWithinTheirRanges
// above pins the same property directly against the header, and covers
// kFrameRate as well. The other twelve savers, whose defaults are literals,
// each carry the SettingsWithinDeclaredRanges form instead.

// --- the BLOCKER guard -----------------------------------------------------
//
// SonarCloud reports cpp:S3519 at cyclone.cpp:155 and :163: a heap access at a
// negative byte offset, and an index past the end. Both index xyz[], which
// initSaver allocates as new float*[dComplexity+3] and then walks down from
// dComplexity+2. Reaching them needs a negative dComplexity.
//
// That cannot happen: screenSaverProc calls readRegistry() before initSaver(),
// and both the default and every successful registry read take their bounds
// from cycloneSettings.h before initSaver can allocate from dComplexity.
//
// cycloneSettings.h now holds the clamp as pure logic, so the dangerous inputs
// are exercised directly above without modifying the developer's registry.
//
// Setting-to-constant pairing (dComplexity clamped against kComplexity, not
// some other saver's range of the same shape) is enforced separately by the
// SettingsClampWiring ctest case (tests/tools/check-settings-wiring.cmake),
// which is the only check that would catch a mis-paired constant - a runtime
// assertion here cannot, because a wrong-but-similarly-shaped range can still
// pass a range check.

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

// No existing framework-level readRegistry test to rename: cyclone's
// readRegistry coverage is the CycloneBlockerGuard suite above, which is left
// byte-for-byte untouched. This is new, and is the cyclone member of the
// per-saver postcondition family every saver now carries.
//
// cyclone's two flags are not in the range list: unlike the other twelve
// savers, which clamp their checkboxes against a {0,1} Range, cyclone runs
// them through cycloneSettings::normalizeFlag, so they are asserted as flags
// rather than as bounded values.
TEST(CycloneFramework, ReadRegistryLeavesEverySettingInsideItsDeclaredRange) {
    dCyclones = -1;
    dParticles = 100000;
    dSize = -1;
    dComplexity = -1;
    dSpeed = 100000;
    dStretch = -1;
    dShowCurves = 100000;

    readRegistry();

    EXPECT_TRUE(savertest::SettingsWithinDeclaredRanges({
        savertest::Ranged("dCyclones", dCyclones, cycloneSettings::kCyclones),
        savertest::Ranged("dParticles", dParticles, cycloneSettings::kParticles),
        savertest::Ranged("dSize", dSize, cycloneSettings::kSize),
        savertest::Ranged("dComplexity", dComplexity, cycloneSettings::kComplexity),
        savertest::Ranged("dSpeed", dSpeed, cycloneSettings::kSpeed),
    }));
    EXPECT_TRUE(dStretch == 0 || dStretch == 1) << "dStretch = " << dStretch;
    EXPECT_TRUE(dShowCurves == 0 || dShowCurves == 1) << "dShowCurves = " << dShowCurves;
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

TEST(CycloneDialogs, AboutProcHandlesTheStandardCloseCommand) {
    EXPECT_TRUE(aboutProc(nullptr, WM_COMMAND, IDCANCEL, 0));
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
    dFrameRateLimit = 144;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS, 0);

    EXPECT_EQ(dParticles, defaultParticles);
    EXPECT_EQ(dFrameRateLimit, (unsigned int)cycloneSettings::kDefaultFrameRate);
}
