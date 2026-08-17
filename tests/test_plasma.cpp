/*
 * Tests for the plasma saver.
 *
 * plasma.cpp is compiled into this binary against the recording GL stub, so the
 * real draw path runs headless. Nothing here can tell you plasma looks right -
 * only that it issues a coherent sequence of GL commands and that its settings
 * maths behaves. Shared scaffolding lives in support/saver_test_common.h.
 */

#include "support/saver_test_common.h"


#include "resource.h"

// plasma.cpp has no header; its contract with the framework is by name.
// SonarCloud cpp:S5421 flags these as mutable globals; they are declarations of
// the saver's own, which is Task 6 in docs/MAINTENANCE.md.
extern int dZoom;
extern int dFocus;
extern int dSpeed;
extern int dResolution;
extern int plasmasize;
extern int readyToDraw;
extern int kStatistics;   // owned by the shim, toggled by the s key in a real saver
extern float aspectRatio;
extern float wide;
extern float high;

void setDefaults();
void setPlasmaSize();
void readRegistry();
void initControls(HWND hdlg);
LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

namespace {

class Plasma : public savertest::SaverFixture {
protected:
    void SetUp() override {
        rsRandGen().seed(savertest::kTestSeed);
        setDefaults();
    }
};

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(PlasmaHarness, SaverBodyWasActuallyCompiled) {
    // Guards the WIN32-define trap. MSVC predefines _WIN32, not WIN32, and the
    // entire saver sits inside #ifdef WIN32. Without -DWIN32 this file links
    // against an empty translation unit and every other test passes vacuously
    // while coverage reports zero.
    setDefaults();
    EXPECT_EQ(dZoom, 10);
    EXPECT_EQ(dFocus, 30);
    EXPECT_EQ(dSpeed, 20);
    EXPECT_EQ(dResolution, 25);
}

// --- settings maths --------------------------------------------------------

TEST(PlasmaSettings, PlasmaSizeScalesWithResolution) {
    setDefaults();
    aspectRatio = 1.0f;

    // Not named `small`/`large`: Windows.h drags in rpcndr.h, which does
    // `#define small char`.
    dResolution = 10;
    setPlasmaSize();
    const int coarseSize = plasmasize;

    dResolution = 50;
    setPlasmaSize();
    const int fineSize = plasmasize;

    EXPECT_GT(coarseSize, 0);
    EXPECT_GT(fineSize, coarseSize) << "a higher resolution setting must mean more samples";
}

TEST(PlasmaSettings, ZoomDrivesTheVisibleExtent) {
    setDefaults();
    aspectRatio = 1.0f;

    dZoom = 5;
    setPlasmaSize();
    const float zoomedOut = wide;

    dZoom = 50;
    setPlasmaSize();
    const float zoomedIn = wide;

    EXPECT_GT(zoomedOut, zoomedIn) << "wide is 30/dZoom, so a larger dZoom narrows the view";
    EXPECT_GT(zoomedIn, 0.0f);
}

TEST(PlasmaSettings, WidescreenStretchesWidthAndPortraitStretchesHeight) {
    setDefaults();

    aspectRatio = 2.0f;   // landscape: height derives from width
    setPlasmaSize();
    EXPECT_GT(wide, high);

    aspectRatio = 0.5f;   // portrait: the other branch
    setPlasmaSize();
    EXPECT_GT(high, wide);
}

// --- a frame ---------------------------------------------------------------

TEST_F(Plasma, InitSaverPreparesTextureAndMarksReady) {
    glstub::reset();
    initSaver(hostWindow());

    const glstub::Trace& t = glstub::trace();
    EXPECT_EQ(readyToDraw, 1);
    EXPECT_GE(t.texturesGenerated, 1) << "plasma uploads one texture at startup";
    EXPECT_GE(t.countCalls("glTexImage2D"), 1);
    EXPECT_GT(plasmasize, 0) << "aspectRatio came from the host window, so sizing must have run";

    cleanUp(hostWindow());
}

TEST_F(Plasma, FrameLeavesTheMatrixStackBalanced) {
    start();
    draw();
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

TEST_F(Plasma, FramePairsBeginAndEnd) {
    start();
    draw();
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

TEST_F(Plasma, PrimitiveVertexCountsAreLegal) {
    start();
    draw();
    EXPECT_TRUE(savertest::VertexCountsLegal());
}

TEST_F(Plasma, DoesNotLeakEnableState) {
    start();
    draw();
    EXPECT_TRUE(savertest::NoEnableStateLeaked());
}

TEST_F(Plasma, EmitsExactlyOneTexturedStripRegardlessOfResolution) {
    // plasma computes its field on the CPU, uploads it as a texture and blits a
    // single full-screen primitive. Geometry is therefore constant: resolution
    // changes the texture, never the vertex count. Pinned because it is the
    // opposite of what most savers do, and easy to "fix" by mistake.
    dResolution = 10;
    start();
    draw();
    EXPECT_EQ(glstub::trace().totalVertices(), 4u);

    stop();
    dResolution = 40;
    start();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_EQ(t.totalVertices(), 4u);
    ASSERT_EQ(t.primitives.size(), 1u) << "one screen-filling primitive per frame";
    EXPECT_EQ(t.primitives[0].mode, static_cast<unsigned>(GL_TRIANGLE_STRIP))
        << "plasma.cpp:169 uses a 4-vertex strip, not GL_QUADS";
}

TEST_F(Plasma, UploadsTheFieldAndPresentsOncePerFrame) {
    start();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_GE(t.countCalls("glTexSubImage2D"), 1) << "the computed field must reach the texture";
    EXPECT_EQ(t.countCalls("wglSwapLayerBuffers"), 1) << "exactly one present per frame";
}

TEST_F(Plasma, StatisticsOverlayKeepsTheMatrixStackBalanced) {
    // The kStatistics branch pushes on both PROJECTION and MODELVIEW and pops
    // them in the reverse order. It is off by default, so nothing else covers it.
    start();
    kStatistics = 1;
    draw();
    kStatistics = 0;

    EXPECT_TRUE(savertest::MatrixStackBalanced());
    EXPECT_GE(glstub::trace().pushes, 2) << "the overlay should have pushed both matrix modes";
}

// --- framework entry points ------------------------------------------------

TEST_F(Plasma, IdleProcSkipsDrawingWhenNotReady) {
    start();
    readyToDraw = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().totalVertices(), 0u)
        << "idleProc must not draw before the saver is ready";
    readyToDraw = 1;
}

TEST(PlasmaFramework, ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy) {
    setDefaults();
    readyToDraw = 0;
    glstub::reset();

    screenSaverProc(testsupport::hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(readyToDraw, 1);
    EXPECT_GE(glstub::trace().texturesGenerated, 1);

    screenSaverProc(testsupport::hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(readyToDraw, 0);
}

TEST(PlasmaFramework, ScreenSaverProcPassesUnhandledMessagesThrough) {
    EXPECT_NO_FATAL_FAILURE(screenSaverProc(testsupport::hostWindow(), WM_PAINT, 0, 0));
}

TEST(PlasmaFramework, ReadRegistryLeavesEveryValueUsable) {
    // readRegistry only reads: it calls setDefaults first and returns early if
    // the key is absent, so running it cannot disturb the machine. That early
    // return also means it covers little where the saver has never stored
    // settings, CI included - see the note in test_cyclone.cpp.
    //
    // This is the unclamped read Task 11 targets, so it asserts non-degeneracy
    // rather than range.
    readRegistry();

    EXPECT_NE(dZoom, 0) << "dZoom divides into 30.0f in setPlasmaSize";
    EXPECT_GT(dResolution, 0);
    EXPECT_GT(dSpeed, 0);
}

// --- dialog procedures -----------------------------------------------------

TEST(PlasmaDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_TRUE(savertest::AboutProcColoursTheWebPageLabel(aboutProc));
}

TEST(PlasmaDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(aboutProc));
}

TEST(PlasmaDialogs, InitControlsRunsWithoutADialog) {
    setDefaults();
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(PlasmaDialogs, ConfigureDialogHandlesTheStandardMessages) {
    EXPECT_TRUE(savertest::ConfigureDialogInitialisesAndCancels(screenSaverConfigureDialog));
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(screenSaverConfigureDialog));
}

TEST(PlasmaDialogs, ConfigureDialogRestoresDefaults) {
    setDefaults();
    const int defaultZoom = dZoom;
    dZoom = 99;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS, 0);

    EXPECT_EQ(dZoom, defaultZoom) << "the Defaults button must reset the settings";
}
