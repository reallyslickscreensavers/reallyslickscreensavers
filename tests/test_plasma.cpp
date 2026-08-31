/*
 * Tests for the plasma saver.
 *
 * plasma.cpp is compiled into this binary against the recording GL stub, so the
 * real draw path runs headless. Nothing here can tell you plasma looks right -
 * only that it issues a coherent sequence of GL commands and that its settings
 * maths behaves. Shared scaffolding lives in support/saver_test_common.h.
 */

#include "support/saver_test_common.h"

// For kStatistics, which the framework owns and the shim defines. Declaring it
// here instead would be a mutable global of our own (cpp:S5421); the header is
// outside the analysed sources.
#include <rsWin32Saver/rsWin32Saver.h>

#include "resource.h"
#include "plasmaSettings.h"
// The saver's module state, reached through its accessor rather than through
// externs of our own (Task 6 in docs/MAINTENANCE.md).
#include "plasmaState.h"

using plasmaState::state;

// plasma.cpp has no header; its contract with the framework is by name.
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

// The settings this saver reads and the ranges its own header declares.
// Built once: both cases below assert against the same list, and writing it
// out twice is what tripped the duplication gate.
std::vector<savertest::RangedSetting> declaredRanges() {
    return {
        savertest::Ranged("dZoom", state().dZoom, plasmaSettings::kZoom),
        savertest::Ranged("dFocus", state().dFocus, plasmaSettings::kFocus),
        savertest::Ranged("dSpeed", state().dSpeed, plasmaSettings::kSpeed),
        savertest::Ranged("dResolution", state().dResolution, plasmaSettings::kResolution),
    };
}

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(PlasmaHarness, SaverBodyWasActuallyCompiled) {
    // Guards the WIN32-define trap. MSVC predefines _WIN32, not WIN32, and the
    // entire saver sits inside #ifdef WIN32. Without -DWIN32 this file links
    // against an empty translation unit and every other test passes vacuously
    // while coverage reports zero.
    setDefaults();
    EXPECT_EQ(state().dZoom, 10);
    EXPECT_EQ(state().dFocus, 30);
    EXPECT_EQ(state().dSpeed, 20);
    EXPECT_EQ(state().dResolution, 25);
}

TEST(PlasmaHarness, DefaultsSitInsideTheDeclaredRanges) {
    // The header declares the ranges and the saver picks the defaults; nothing
    // else checks that the two agree.
    setDefaults();
    EXPECT_TRUE(savertest::SettingsWithinDeclaredRanges(declaredRanges()));
}

// --- settings maths --------------------------------------------------------

TEST(PlasmaSettings, PlasmaSizeScalesWithResolution) {
    setDefaults();
    state().aspectRatio = 1.0f;

    // Not named `small`/`large`: Windows.h drags in rpcndr.h, which does
    // `#define small char`.
    state().dResolution = 10;
    setPlasmaSize();
    const int coarseSize = state().plasmasize;

    state().dResolution = 50;
    setPlasmaSize();
    const int fineSize = state().plasmasize;

    EXPECT_GT(coarseSize, 0);
    EXPECT_GT(fineSize, coarseSize) << "a higher resolution setting must mean more samples";
}

TEST(PlasmaSettings, ZoomDrivesTheVisibleExtent) {
    setDefaults();
    state().aspectRatio = 1.0f;

    state().dZoom = 5;
    setPlasmaSize();
    const float zoomedOut = state().wide;

    state().dZoom = 50;
    setPlasmaSize();
    const float zoomedIn = state().wide;

    EXPECT_GT(zoomedOut, zoomedIn) << "wide is 30/dZoom, so a larger dZoom narrows the view";
    EXPECT_GT(zoomedIn, 0.0f);
}

TEST(PlasmaSettings, WidescreenStretchesWidthAndPortraitStretchesHeight) {
    setDefaults();

    state().aspectRatio = 2.0f;   // landscape: height derives from width
    setPlasmaSize();
    EXPECT_GT(state().wide, state().high);

    state().aspectRatio = 0.5f;   // portrait: the other branch
    setPlasmaSize();
    EXPECT_GT(state().high, state().wide);
}

// --- a frame ---------------------------------------------------------------

TEST_F(Plasma, InitSaverPreparesTextureAndMarksReady) {
    glstub::reset();
    initSaver(hostWindow());

    const glstub::Trace& t = glstub::trace();
    EXPECT_EQ(state().readyToDraw, 1);
    EXPECT_GE(t.texturesGenerated, 1) << "plasma uploads one texture at startup";
    EXPECT_GE(t.countCalls("glTexImage2D"), 1);
    EXPECT_GT(state().plasmasize, 0) << "aspectRatio came from the host window, so sizing must have run";

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
    state().dResolution = 10;
    start();
    draw();
    EXPECT_EQ(glstub::trace().totalVertices(), 4u);

    stop();
    state().dResolution = 40;
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
    state().readyToDraw = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().totalVertices(), 0u)
        << "idleProc must not draw before the saver is ready";
    state().readyToDraw = 1;
}

TEST(PlasmaFramework, ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy) {
    setDefaults();
    state().readyToDraw = 0;
    glstub::reset();

    screenSaverProc(testsupport::hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(state().readyToDraw, 1);
    EXPECT_GE(glstub::trace().texturesGenerated, 1);

    screenSaverProc(testsupport::hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(state().readyToDraw, 0);
}

TEST(PlasmaFramework, ScreenSaverProcPassesUnhandledMessagesThrough) {
    EXPECT_NO_FATAL_FAILURE(screenSaverProc(testsupport::hostWindow(), WM_PAINT, 0, 0));
}

TEST(PlasmaFramework, ReadRegistryLeavesEverySettingInsideItsDeclaredRange) {
    // readRegistry only reads: it calls setDefaults first and returns early if
    // the key is absent, so running it cannot disturb the machine. That early
    // return also means the clamp itself covers little where the saver has
    // never stored settings, CI included - see the note in test_cyclone.cpp.
    //
    // kZoom.lo == 1 is what makes EXPECT_NE(dZoom, 0) below structural rather
    // than incidental: dZoom divides into 30.0f in setPlasmaSize.
    readRegistry();

    EXPECT_NE(state().dZoom, 0) << "dZoom divides into 30.0f in setPlasmaSize";
    EXPECT_TRUE(savertest::SettingsWithinDeclaredRanges(declaredRanges()));
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
    const int defaultZoom = state().dZoom;
    state().dZoom = 99;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS, 0);

    EXPECT_EQ(state().dZoom, defaultZoom) << "the Defaults button must reset the settings";
}
