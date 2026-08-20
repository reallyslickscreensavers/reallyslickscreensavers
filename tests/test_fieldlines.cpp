/*
 * Tests for the fieldlines saver.
 *
 * fieldlines.cpp is compiled into this binary against the recording GL stub, so
 * the real draw path runs headless. Shared scaffolding - the fixture and the
 * frame invariants - lives in support/saver_test_common.h; what is here is what
 * is specific to fieldlines.
 */

#include "support/saver_test_common.h"


#include "resource.h"
#include "fieldlinesSettings.h"

// fieldlines.cpp has no header; its contract with the framework is by name.
//
// SonarCloud cpp:S5421 flags these as mutable globals. They are declarations,
// not definitions - the variables live in fieldlines.cpp - but the rule cannot
// tell the difference. The finding is really about the savers exposing their
// settings as mutable globals at all, which is Task 6 in docs/MAINTENANCE.md.
extern int dIons;
extern int dStepSize;
extern int dMaxSteps;
extern int dWidth;
extern int dSpeed;
extern BOOL dConstwidth;
extern BOOL dElectric;
extern int readyToDraw;

void setDefaults();
void readRegistry();
void initControls(HWND hdlg);
LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

namespace {

class Fieldlines : public savertest::SaverFixture {
protected:
    void SetUp() override {
        rsRandGen().seed(savertest::kTestSeed);
        setDefaults();

        // Each field line walks up to dMaxSteps segments (fieldlines.cpp:174),
        // and in the default mode opens a primitive on every one of them - so
        // the shipped 300, across eight lines per ion, is tens of thousands of
        // recorded primitives a frame. A hundred still walks lines, terminates
        // some early on hitting an ion, and opens a strip per step the same way.
        // The default is asserted in the harness test.
        dMaxSteps = 100;
    }
};

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(FieldlinesHarness, SaverBodyWasActuallyCompiled) {
    // Guards the WIN32-define trap. MSVC predefines _WIN32, not WIN32, and the
    // whole saver sits inside #ifdef WIN32; without it this links against an
    // empty translation unit and every other test passes vacuously.
    setDefaults();
    EXPECT_EQ(dIons, 6);
    EXPECT_EQ(dStepSize, 10);
    EXPECT_EQ(dMaxSteps, 300);
    EXPECT_EQ(dWidth, 30);
}

TEST(FieldlinesHarness, DefaultsSitInsideTheDeclaredRanges) {
    // The header declares the ranges and the saver picks the defaults; nothing
    // else checks that the two agree.
    setDefaults();
    EXPECT_TRUE(savertest::SettingsWithinDeclaredRanges({
        savertest::Ranged("dIons", dIons, fieldlinesSettings::kIons),
        savertest::Ranged("dStepSize", dStepSize, fieldlinesSettings::kStepSize),
        savertest::Ranged("dMaxSteps", dMaxSteps, fieldlinesSettings::kMaxSteps),
        savertest::Ranged("dWidth", dWidth, fieldlinesSettings::kWidth),
        savertest::Ranged("dSpeed", dSpeed, fieldlinesSettings::kSpeed),
        savertest::Ranged("dConstwidth", dConstwidth, fieldlinesSettings::kConstwidth),
        savertest::Ranged("dElectric", dElectric, fieldlinesSettings::kElectric),
    }));
}

// --- a frame ---------------------------------------------------------------

TEST_F(Fieldlines, FrameLeavesTheMatrixStackBalanced) {
    start();
    draw();
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

TEST_F(Fieldlines, PrimitiveVertexCountsAreLegal) {
    start();
    draw();
    EXPECT_TRUE(savertest::VertexCountsLegal());
}

TEST_F(Fieldlines, DoesNotLeakEnableState) {
    start();
    draw();
    EXPECT_TRUE(savertest::NoEnableStateLeaked());
}

TEST_F(Fieldlines, DrawsFieldLinesAsStrips) {
    // GL_LINE_STRIP is the only primitive the saver emits.
    start();
    draw();

    EXPECT_GT(countPrimitives(GL_LINE_STRIP), 0);
    EXPECT_GT(glstub::trace().totalVertices(), 0u);
}

TEST_F(Fieldlines, PairsBeginAndEndInBothWidthModes) {
    // Task 15. With dConstwidth false - the default the fixture's setDefaults
    // restores - drawfieldline opens a GL_LINE_STRIP per step so it can vary
    // glLineWidth, and now closes each one before opening the next. It used to
    // close only on the final step, so every intermediate glBegin landed inside
    // an open block and a driver discarded it, and the glLineWidth calls with it.
    //
    // Constant-width mode draws each line as one long strip and was always
    // correct; it is asserted here to show the fix left it alone.
    start();
    draw();
    EXPECT_TRUE(savertest::PrimitivesPaired()) << "default mode, a strip per segment";

    stop();
    dConstwidth = TRUE;
    start();
    draw();
    EXPECT_TRUE(savertest::PrimitivesPaired()) << "constant-width mode, one strip per line";
}

TEST_F(Fieldlines, DefaultModeSetsLineWidthOncePerStrip) {
    // The point of the default mode: each segment's own width applies, which
    // needs the previous strip closed first. The stub records the call and not
    // the value, so "applies" here is one glLineWidth per glBegin with nothing
    // nested - the pre-loop segment and every step both set a width before
    // opening their strip, and nothing else in the frame touches line width.
    start();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_FALSE(t.nestedBeginSeen);
    EXPECT_GT(t.begins, 1) << "the default mode should open many strips, not one";
    EXPECT_EQ(t.countCalls("glLineWidth"), t.begins);

    // A line that reaches an ion stops early, so a frame can only ever open
    // fewer strips than the one-before-the-loop-plus-one-per-step maximum.
    // Hitting the maximum exactly means no line terminated early this frame,
    // which is seed-dependent and not asserted either way.
    EXPECT_LE(t.begins, 8 * dIons * (int(dMaxSteps) + 1));
}

// --- settings change what is drawn -----------------------------------------

TEST_F(Fieldlines, DrawsEightFieldLinesPerIon) {
    // draw() seeds eight lines per ion, one towards each corner
    // (fieldlines.cpp:280-288). Asserted in constant-width mode because that is
    // the one that opens exactly one strip per line.
    //
    // Counting strips in the default mode instead would be flaky: a line stops
    // early when it reaches an ion, so more ions can mean fewer strips, and the
    // PRNG is seeded from std::random_device since rslibs L4.
    dConstwidth = TRUE;
    dIons = 2;
    start();
    draw();
    EXPECT_EQ(countPrimitives(GL_LINE_STRIP), 8 * dIons);

    stop();
    dIons = 5;
    start();
    draw();
    EXPECT_EQ(countPrimitives(GL_LINE_STRIP), 8 * dIons);
}

TEST_F(Fieldlines, ElectricModeStillDraws) {
    dElectric = TRUE;
    start();
    draw();

    EXPECT_GT(glstub::trace().totalVertices(), 0u);
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

// --- framework entry points ------------------------------------------------

TEST_F(Fieldlines, IdleProcSkipsDrawingWhenNotReady) {
    start();
    readyToDraw = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().totalVertices(), 0u);
    readyToDraw = 1;
}

TEST(FieldlinesFramework, ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy) {
    setDefaults();
    readyToDraw = 0;

    screenSaverProc(testsupport::hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(readyToDraw, 1);

    screenSaverProc(testsupport::hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(readyToDraw, 0);
}

TEST(FieldlinesFramework, ReadRegistryLeavesEverySettingInsideItsDeclaredRange) {
    // Read-only: setDefaults runs first and the function returns early if the
    // key is absent, so this cannot disturb the machine. That early return also
    // means the clamp itself covers little where the saver has never stored
    // settings, CI included - see the note in test_cyclone.cpp.
    readRegistry();

    EXPECT_TRUE(savertest::SettingsWithinDeclaredRanges({
        savertest::Ranged("dIons", dIons, fieldlinesSettings::kIons),
        savertest::Ranged("dStepSize", dStepSize, fieldlinesSettings::kStepSize),
        savertest::Ranged("dMaxSteps", dMaxSteps, fieldlinesSettings::kMaxSteps),
        savertest::Ranged("dWidth", dWidth, fieldlinesSettings::kWidth),
        savertest::Ranged("dSpeed", dSpeed, fieldlinesSettings::kSpeed),
        savertest::Ranged("dConstwidth", dConstwidth, fieldlinesSettings::kConstwidth),
        savertest::Ranged("dElectric", dElectric, fieldlinesSettings::kElectric),
    }));
}

// --- dialog procedures -----------------------------------------------------

TEST(FieldlinesDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_TRUE(savertest::AboutProcColoursTheWebPageLabel(aboutProc));
}

TEST(FieldlinesDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(aboutProc));
}

TEST(FieldlinesDialogs, InitControlsRunsWithoutADialog) {
    setDefaults();
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(FieldlinesDialogs, ConfigureDialogHandlesTheStandardMessages) {
    EXPECT_TRUE(savertest::ConfigureDialogInitialisesAndCancels(screenSaverConfigureDialog));
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(screenSaverConfigureDialog));
}

TEST(FieldlinesDialogs, ConfigureDialogRestoresDefaults) {
    setDefaults();
    const int defaultIons = dIons;
    dIons = 99;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS, 0);

    EXPECT_EQ(dIons, defaultIons);
}
