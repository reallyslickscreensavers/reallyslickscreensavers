/*
 * Tests for the fieldlines saver.
 *
 * fieldlines.cpp is compiled into this binary against the recording GL stub, so
 * the real draw path runs headless. Nothing here can tell you it looks right -
 * only that it issues a coherent command stream and that its settings drive it.
 */

#include <gtest/gtest.h>

#include <Windows.h>
#include <gl/GL.h>

#include "support/gl_stub.h"
#include "support/test_window.h"
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
extern int dSpeed;
extern BOOL dConstwidth;
extern BOOL dElectric;
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

class Fieldlines : public ::testing::Test {
protected:
    void SetUp() override { setDefaults(); }
    void TearDown() override { if (started_) cleanUp(hostWindow()); }

    // Discards the first frame so every test measures a warm one. ctest runs
    // each case in its own process, so anything a cold frame does once would
    // otherwise make a test pass in suite order and fail in isolation.
    void start() {
        initSaver(hostWindow());
        started_ = true;
        draw();           // warm-up
        glstub::reset();
    }

    // Settings change only while nothing is allocated, so stopping is a
    // separate step. See the note in test_solarwinds.cpp: a saver whose
    // destructor sizes its frees from the current globals corrupts the heap if
    // a count changes underneath it.
    void stop() {
        if (started_) {
            cleanUp(hostWindow());
            started_ = false;
        }
    }
private:
    bool started_ = false;
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

    const glstub::Trace& t = glstub::trace();
    EXPECT_TRUE(t.matrixBalanced())
        << "depth " << t.matrixDepth << ", " << t.pushes << " pushes vs " << t.pops << " pops";
    EXPECT_GE(t.minMatrixDepth, 0);
}

TEST_F(Fieldlines, ConstantWidthModePairsBeginAndEnd) {
    // dConstwidth draws each field line as one long strip, which is the mode
    // where the begin/end structure is correct.
    dConstwidth = TRUE;
    start();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_TRUE(t.primitivesBalanced()) << t.begins << " glBegin vs " << t.ends << " glEnd";
    EXPECT_FALSE(t.nestedBeginSeen);
    EXPECT_FALSE(t.vertexOutsideBegin);
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
    // change and belongs with the reliability work in docs/MAINTENANCE.md, not
    // in a test-only change.
    dConstwidth = FALSE;
    start();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_TRUE(t.nestedBeginSeen)
        << "if this now fails the defect is fixed - delete this test and enable the balanced one";
    EXPECT_GT(t.begins, t.ends) << "more glBegin than glEnd is the signature of it";
    EXPECT_FALSE(t.vertexOutsideBegin) << "vertices at least stay inside a block";
}

TEST_F(Fieldlines, PrimitiveVertexCountsAreLegal) {
    start();
    draw();

    std::string why;
    EXPECT_TRUE(glstub::primitiveVertexCountsLegal(&why)) << why;
}

TEST_F(Fieldlines, DrawsFieldLinesAsStrips) {
    // Every field line is a GL_LINE_STRIP, which is the only primitive the
    // saver emits.
    start();
    draw();

    EXPECT_GT(countPrimitives(GL_LINE_STRIP), 0);
    EXPECT_GT(glstub::trace().totalVertices(), 0u);
}

TEST_F(Fieldlines, DoesNotLeakEnableState) {
    start();
    draw();
    for (const auto& [capability, net] : glstub::trace().enables) {
        EXPECT_EQ(net, 0) << "capability " << capability << " left with net enable " << net;
    }
}

// --- settings change what is drawn -----------------------------------------

TEST_F(Fieldlines, DrawsEightFieldLinesPerIon) {
    // draw() seeds eight lines per ion, one towards each corner
    // (fieldlines.cpp:280-288). Asserted in constant-width mode because that is
    // the one that opens exactly one strip per line.
    //
    // Counting strips in the default mode instead would be flaky: a line stops
    // early when it reaches an ion, and more ions means earlier stops, so a
    // larger dIons can produce fewer strips. The PRNG is seeded from
    // std::random_device since rslibs L4, so that varies run to run.
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

TEST_F(Fieldlines, ConstantWidthStillDraws) {
    // dConstwidth switches the line-width calculation; the geometry must
    // survive either way.
    dConstwidth = TRUE;
    start();
    draw();

    EXPECT_GT(glstub::trace().totalVertices(), 0u);
    std::string why;
    EXPECT_TRUE(glstub::primitiveVertexCountsLegal(&why)) << why;
}

TEST_F(Fieldlines, ElectricModeStillDraws) {
    dElectric = TRUE;
    start();
    draw();

    EXPECT_GT(glstub::trace().totalVertices(), 0u);
    EXPECT_TRUE(glstub::trace().matrixBalanced());
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

    screenSaverProc(hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(readyToDraw, 1);

    screenSaverProc(hostWindow(), WM_DESTROY, 0, 0);
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
//
// IDOK is never sent: it calls writeRegistry and would rewrite real settings.

TEST(FieldlinesDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_NE(aboutProc(nullptr, WM_CTLCOLORSTATIC, 0, 0), 0);
}

TEST(FieldlinesDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_EQ(aboutProc(nullptr, WM_MOUSEMOVE, 0, 0), FALSE);
}

TEST(FieldlinesDialogs, InitControlsRunsWithoutADialog) {
    setDefaults();
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(FieldlinesDialogs, ConfigureDialogInitialisesAndCancels) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_INITDIALOG, 0, 0), TRUE);
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_COMMAND, IDCANCEL, 0), TRUE);
}

TEST(FieldlinesDialogs, ConfigureDialogRestoresDefaults) {
    setDefaults();
    const int defaultIons = dIons;
    dIons = 99;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS, 0);

    EXPECT_EQ(dIons, defaultIons);
}

TEST(FieldlinesDialogs, ConfigureDialogHandlesSliderMovement) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_HSCROLL, 0, 0), TRUE);
}

TEST(FieldlinesDialogs, ConfigureDialogIgnoresUnknownMessages) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_MOUSEMOVE, 0, 0), FALSE);
}
