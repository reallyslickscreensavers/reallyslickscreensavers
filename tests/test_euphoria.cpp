/*
 * Tests for the euphoria saver.
 *
 * euphoria.cpp is compiled into this binary against the recording GL stub, so
 * the real draw path runs headless. Shared scaffolding - the fixture and the
 * frame invariants - lives in support/saver_test_common.h; what is here is what
 * is specific to euphoria.
 */

#include "support/saver_test_common.h"


#include "resource.h"

// euphoria.cpp has no header; its contract with the framework is by name. See
// the note on cpp:S5421 in test_fieldlines.cpp - these are declarations, not
// definitions.
extern int dWisps;
extern int dBackground;
extern int dDensity;
extern int dVisibility;
extern int dFeedback;
extern int dWireframe;
extern int dTexture;
extern int readyToDraw;

void setDefaults(int which);
void readRegistry();
void initControls(HWND hdlg);
LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

namespace {

// DEFAULTS1 is "Regular", euphoria's baseline preset.
class Euphoria : public savertest::SaverFixture {
protected:
    void SetUp() override {
        setDefaults(DEFAULTS1);

        // Each wisp is a (dDensity + 1) squared mesh (euphoria.cpp:116-119), so
        // the shipped 35 costs 1,296 vertices per wisp per frame and the cost
        // falls away quadratically. Twelve still draws a real mesh and reaches
        // the same code; the default itself is asserted in the harness test,
        // and the two cases that compare densities set their own values.
        dDensity = 12;
    }
};

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(EuphoriaHarness, SaverBodyWasActuallyCompiled) {
    // Guards the WIN32-define trap. MSVC predefines _WIN32, not WIN32, and the
    // whole saver sits inside #ifdef WIN32; without it this links against an
    // empty translation unit and every other test passes vacuously.
    setDefaults(DEFAULTS1);
    EXPECT_EQ(dWisps, 5);
    EXPECT_EQ(dBackground, 0);
    EXPECT_EQ(dDensity, 35);
    EXPECT_EQ(dVisibility, 35);
    EXPECT_EQ(dTexture, 4);
}

TEST(EuphoriaHarness, EveryPresetLeavesTheSettingsUsable) {
    const int presets[] = {DEFAULTS1, DEFAULTS2, DEFAULTS3, DEFAULTS4,
                           DEFAULTS5, DEFAULTS6, DEFAULTS7};
    for (int preset : presets) {
        setDefaults(preset);
        EXPECT_GT(dWisps + dBackground, 0) << "preset " << preset << ": nothing would draw";
        EXPECT_GT(dDensity, 0) << "preset " << preset << ": the mesh is sized from this";
        EXPECT_GE(dFeedback, 0) << "preset " << preset;
    }
}

// --- a frame ---------------------------------------------------------------

TEST_F(Euphoria, FrameLeavesTheMatrixStackBalanced) {
    start();
    draw();
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

TEST_F(Euphoria, FramePairsBeginAndEnd) {
    start();
    draw();
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

TEST_F(Euphoria, PrimitiveVertexCountsAreLegal) {
    start();
    draw();
    EXPECT_TRUE(savertest::VertexCountsLegal());
}

TEST_F(Euphoria, DoesNotLeakEnableState) {
    start();
    draw();
    EXPECT_TRUE(savertest::NoEnableStateLeaked());
}

TEST_F(Euphoria, ReadsBackNoInvalidEnums) {
    start();
    draw();
    EXPECT_TRUE(savertest::NoInvalidEnums());
}

TEST_F(Euphoria, DrawsWispsAsTriangleStrips) {
    start();
    draw();

    EXPECT_GT(countPrimitives(GL_TRIANGLE_STRIP), 0);
    EXPECT_GT(glstub::trace().totalVertices(), 0u);
}

// --- settings change what is drawn -----------------------------------------

TEST_F(Euphoria, WireframeModeDrawsLineStripsInsteadOfTriangles) {
    // Each wisp is drawn either as a filled mesh or as its two families of
    // grid lines (euphoria.cpp:234-254).
    stop();
    dWireframe = 1;
    start();
    draw();

    EXPECT_GT(countPrimitives(GL_LINE_STRIP), 0);
    EXPECT_EQ(countPrimitives(GL_TRIANGLE_STRIP), 0);
}

TEST_F(Euphoria, MoreWispsMeansMoreDrawing) {
    // Settings are changed only while nothing is allocated - see Task 16 in
    // docs/MAINTENANCE.md.
    stop();
    dWisps = 2;
    start();
    draw();
    const int few = countPrimitives(GL_TRIANGLE_STRIP);

    stop();
    dWisps = 8;
    start();
    draw();
    const int many = countPrimitives(GL_TRIANGLE_STRIP);

    EXPECT_GT(few, 0);
    EXPECT_GT(many, few);
}

TEST_F(Euphoria, DenserWispsMeansMoreVertices) {
    // dDensity sizes the mesh each wisp is built from, so it changes vertices
    // per strip rather than the number of strips.
    stop();
    dWisps = 1;
    dDensity = 5;
    start();
    draw();
    const unsigned long long coarse = glstub::trace().totalVertices();

    stop();
    dDensity = 30;
    start();
    draw();
    const unsigned long long fine = glstub::trace().totalVertices();

    EXPECT_GT(coarse, 0u);
    EXPECT_GT(fine, coarse);
}

TEST_F(Euphoria, BackgroundWispsAddToTheFrame) {
    stop();
    dBackground = 0;
    start();
    draw();
    const int without = countPrimitives(GL_TRIANGLE_STRIP);

    stop();
    dBackground = 3;
    start();
    draw();
    const int with = countPrimitives(GL_TRIANGLE_STRIP);

    EXPECT_GT(with, without);
}

TEST_F(Euphoria, FeedbackModeCopiesTheFrameIntoATexture) {
    // The feedback pass re-reads the framebuffer into a texture and draws it
    // back twice (euphoria.cpp:326-404). glCopyTexSubImage2D is the tell.
    stop();
    dFeedback = 50;
    start();
    draw();

    EXPECT_GT(glstub::trace().countCalls("glCopyTexSubImage2D"), 0);
    EXPECT_TRUE(savertest::MatrixStackBalanced());
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

// --- framework entry points ------------------------------------------------

TEST_F(Euphoria, IdleProcSkipsDrawingWhenNotReady) {
    start();
    readyToDraw = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().totalVertices(), 0u);
    readyToDraw = 1;
}

TEST(EuphoriaFramework, ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy) {
    setDefaults(DEFAULTS1);
    readyToDraw = 0;

    screenSaverProc(testsupport::hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(readyToDraw, 1);

    screenSaverProc(testsupport::hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(readyToDraw, 0);
}

TEST(EuphoriaFramework, ReadRegistryLeavesEveryValueUsable) {
    // Read-only: setDefaults runs first and the function returns early if the
    // key is absent, so this cannot disturb the machine. That early return also
    // means it covers little where the saver has never stored settings, CI
    // included - see the note in test_cyclone.cpp.
    setDefaults(DEFAULTS1);
    readRegistry();

    EXPECT_GT(dWisps + dBackground, 0);
    EXPECT_GT(dDensity, 0);
    EXPECT_GT(dVisibility, 0);
}

// --- dialog procedures -----------------------------------------------------

TEST(EuphoriaDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_TRUE(savertest::AboutProcColoursTheWebPageLabel(aboutProc));
}

TEST(EuphoriaDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(aboutProc));
}

TEST(EuphoriaDialogs, InitControlsRunsWithoutADialog) {
    setDefaults(DEFAULTS1);
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(EuphoriaDialogs, ConfigureDialogHandlesTheStandardMessages) {
    EXPECT_TRUE(savertest::ConfigureDialogInitialisesAndCancels(screenSaverConfigureDialog));
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(screenSaverConfigureDialog));
}

TEST(EuphoriaDialogs, EveryPresetButtonRestoresItsPreset) {
    const int buttons[] = {DEFAULTS1, DEFAULTS2, DEFAULTS3, DEFAULTS4,
                           DEFAULTS5, DEFAULTS6, DEFAULTS7};
    for (int button : buttons) {
        setDefaults(button);
        const int expected = dWisps;
        dWisps = 99;

        screenSaverConfigureDialog(nullptr, WM_COMMAND, button, 0);

        EXPECT_EQ(dWisps, expected) << "preset button " << button;
    }
}
