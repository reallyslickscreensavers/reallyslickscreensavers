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
        setDefaults();

        // Each field line walks up to dMaxSteps segments (fieldlines.cpp:183),
        // and in the default mode reopens a primitive on every one of them - so
        // the shipped 300, across eight lines per ion, is tens of thousands of
        // recorded primitives a frame. A hundred still walks lines, terminates
        // some early on hitting an ion, and nests glBegin the same way. The
        // default is asserted in the harness test.
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

TEST_F(Fieldlines, ConstantWidthModePairsBeginAndEnd) {
    // dConstwidth draws each field line as one long strip, which is the mode
    // where the begin/end structure is correct.
    dConstwidth = TRUE;
    start();
    draw();
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

TEST_F(Fieldlines, DefaultModeLeavesGlBeginBlocksUnclosed) {
    // DEFECT, pinned rather than fixed - this test documents current behaviour.
    //
    // With dConstwidth false, which is the default, drawfieldline reopens a
    // GL_LINE_STRIP on every step so it can vary glLineWidth per segment
    // (fieldlines.cpp:247-249). But the matching glEnd only runs on the final
    // step (fieldlines.cpp:258-260), so every intermediate glBegin lands inside
    // an already-open block.
    //
    // In a real driver each of those is GL_INVALID_OPERATION and is ignored,
    // along with the glLineWidth calls between them, so the per-segment width
    // the code is reaching for never actually applies and the line renders as
    // one uniform strip. It looks fine, which is why it has survived.
    //
    // Fixing it means closing each strip before reopening; that is a rendering
    // change and belongs with the reliability work in docs/MAINTENANCE.md.
    dConstwidth = FALSE;
    start();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_TRUE(t.nestedBeginSeen)
        << "if this now fails the defect is fixed - delete this test and drop the "
           "dConstwidth line from ConstantWidthModePairsBeginAndEnd";
    EXPECT_GT(t.begins, t.ends) << "more glBegin than glEnd is the signature of it";
    EXPECT_FALSE(t.vertexOutsideBegin) << "vertices at least stay inside a block";
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

TEST(FieldlinesFramework, ReadRegistryLeavesEveryValueUsable) {
    // Read-only: setDefaults runs first and the function returns early if the
    // key is absent, so this cannot disturb the machine. That early return also
    // means it covers little where the saver has never stored settings, CI
    // included - see the note in test_cyclone.cpp.
    readRegistry();

    EXPECT_GT(dIons, 0) << "the ion array is allocated with this count";
    EXPECT_GT(dMaxSteps, 0);
    EXPECT_GT(dStepSize, 0);
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
