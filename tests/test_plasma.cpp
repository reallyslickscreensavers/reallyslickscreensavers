/*
 * Tests for the plasma saver.
 *
 * plasma.cpp is compiled directly into this binary against the recording GL
 * stub, so the real draw path runs headless. Nothing here can tell you plasma
 * looks right — only that it issues a coherent sequence of GL commands and that
 * its settings maths behaves.
 */

#include <gtest/gtest.h>

#include <Windows.h>
#include <gl/GL.h>

#include "support/gl_stub.h"
#include "support/test_window.h"

// The module's own control ids (DEFAULTS, ZOOM, ...). Quoted include: every
// saver has its own resource.h and the target puts src/plasma first.
#include "resource.h"

// plasma.cpp has no header; its contract with the framework is by name.
//
// The savers keep their settings in mutable globals (docs/MAINTENANCE.md
// Task 6). These accessors reach them without this file declaring globals of
// its own: the extern declarations are block-scope, so they bind to the
// definitions in plasma.cpp. They must sit at global scope - inside a
// namespace a block-scope extern would bind to that namespace instead.
static int&   svZoom()        { extern int dZoom;        return dZoom; }
static int&   svFocus()       { extern int dFocus;       return dFocus; }
static int&   svSpeed()       { extern int dSpeed;       return dSpeed; }
static int&   svResolution()  { extern int dResolution;  return dResolution; }
static int&   svFieldSize()   { extern int plasmasize;   return plasmasize; }
static int&   svReadyToDraw() { extern int readyToDraw;  return readyToDraw; }
static int&   svStatistics()  { extern int kStatistics;  return kStatistics; }
static float& svAspectRatio() { extern float aspectRatio; return aspectRatio; }
static float& svWide()        { extern float wide;       return wide; }
static float& svHigh()        { extern float high;       return high; }

void setDefaults();
void setPlasmaSize();
void draw();
void idleProc();
void initSaver(HWND hwnd);
void readRegistry();
void initControls(HWND hdlg);
LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

namespace {

// A fixed-size hidden window, not the desktop: initSaver derives aspectRatio
// from GetClientRect, and plasma sizes its whole field from that. See
// test_window.h - using the desktop made CI cover five points less than local.
HWND hostWindow() { return testsupport::hostWindow(); }

// Brings plasma up the way the framework would, then clears the trace so a test
// sees only what it exercises itself.
void initialiseSaver() {
    setDefaults();
    initSaver(hostWindow());
    glstub::reset();
}

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(PlasmaHarness, SaverBodyWasActuallyCompiled) {
    // Guards the WIN32-define trap. MSVC predefines _WIN32, not WIN32, and the
    // entire saver sits inside #ifdef WIN32. Without -DWIN32 this file links
    // against an empty translation unit and every other test passes vacuously
    // while coverage reports zero.
    setDefaults();
    EXPECT_EQ(svZoom(), 10);
    EXPECT_EQ(svFocus(), 30);
    EXPECT_EQ(svSpeed(), 20);
    EXPECT_EQ(svResolution(), 25);
}

// --- settings maths --------------------------------------------------------

TEST(PlasmaSettings, PlasmaSizeScalesWithResolution) {
    setDefaults();
    svAspectRatio() = 1.0f;

    // Not named `small`/`large`: Windows.h drags in rpcndr.h, which does
    // `#define small char`.
    svResolution() = 10;
    setPlasmaSize();
    const int coarseSize = svFieldSize();

    svResolution() = 50;
    setPlasmaSize();
    const int fineSize = svFieldSize();

    EXPECT_GT(coarseSize, 0);
    EXPECT_GT(fineSize, coarseSize) << "a higher resolution setting must mean more samples";
}

TEST(PlasmaSettings, ZoomDrivesTheVisibleExtent) {
    setDefaults();
    svAspectRatio() = 1.0f;

    svZoom() = 5;
    setPlasmaSize();
    const float zoomedOut = svWide();

    svZoom() = 50;
    setPlasmaSize();
    const float zoomedIn = svWide();

    EXPECT_GT(zoomedOut, zoomedIn) << "wide is 30/dZoom, so a larger dZoom narrows the view";
    EXPECT_GT(zoomedIn, 0.0f);
}

TEST(PlasmaSettings, WideWidescreenKeepsHeightAndStretchesWidth) {
    setDefaults();

    svAspectRatio() = 2.0f;   // landscape: height derives from width
    setPlasmaSize();
    EXPECT_GT(svWide(), svHigh());

    svAspectRatio() = 0.5f;   // portrait: the other branch
    setPlasmaSize();
    EXPECT_GT(svHigh(), svWide());
}

// --- the real draw path, against the recording stub ------------------------

TEST(PlasmaDraw, InitSaverPreparesTextureAndMarksReady) {
    setDefaults();
    glstub::reset();
    initSaver(hostWindow());

    const glstub::Trace& t = glstub::trace();
    EXPECT_EQ(svReadyToDraw(), 1);
    EXPECT_GE(t.texturesGenerated, 1) << "plasma uploads one texture at startup";
    EXPECT_GE(t.countCalls("glTexImage2D"), 1);
    EXPECT_GT(svFieldSize(), 0) << "aspectRatio came from the host window, so sizing must have run";
}

TEST(PlasmaDraw, FrameLeavesTheMatrixStackBalanced) {
    initialiseSaver();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_TRUE(t.matrixBalanced())
        << "depth " << t.matrixDepth << ", " << t.pushes << " pushes vs " << t.pops << " pops";
    EXPECT_GE(t.minMatrixDepth, 0) << "popped further than it pushed";
}

TEST(PlasmaDraw, FramePairsBeginAndEnd) {
    initialiseSaver();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_TRUE(t.primitivesBalanced())
        << t.begins << " glBegin vs " << t.ends << " glEnd";
    EXPECT_FALSE(t.nestedBeginSeen) << "glBegin inside glBegin is always a bug";
    EXPECT_FALSE(t.vertexOutsideBegin) << "vertex emitted outside a glBegin block";
}

TEST(PlasmaDraw, PrimitiveVertexCountsAreLegal) {
    initialiseSaver();
    draw();

    std::string why;
    EXPECT_TRUE(glstub::primitiveVertexCountsLegal(&why)) << why;
}

TEST(PlasmaDraw, FrameEmitsGeometry) {
    initialiseSaver();
    draw();

    EXPECT_GT(glstub::trace().totalVertices(), 0u)
        << "a frame that draws nothing means the saver is broken, not fast";
}

TEST(PlasmaDraw, DoesNotLeakEnableState) {
    initialiseSaver();
    draw();

    // Anything enabled during a frame must be disabled again by the end of it.
    for (const auto& [capability, net] : glstub::trace().enables) {
        EXPECT_EQ(net, 0)
            << "capability " << capability << " left with net enable " << net;
    }
}

TEST(PlasmaDraw, EmitsExactlyOneTexturedQuadRegardlessOfResolution) {
    // plasma computes its field on the CPU, uploads it as a texture and blits a
    // single full-screen quad. Geometry is therefore constant: resolution
    // changes the texture, never the vertex count. Pinned because it is the
    // opposite of what most savers do, and easy to "fix" by mistake.
    setDefaults();
    svResolution() = 10;
    initSaver(hostWindow());
    glstub::reset();
    draw();
    const unsigned long long coarse = glstub::trace().totalVertices();

    setDefaults();
    svResolution() = 40;
    initSaver(hostWindow());
    glstub::reset();
    draw();
    const glstub::Trace& t = glstub::trace();

    EXPECT_EQ(coarse, 4u);
    EXPECT_EQ(t.totalVertices(), 4u);
    ASSERT_EQ(t.primitives.size(), 1u) << "one screen-filling primitive per frame";
    EXPECT_EQ(t.primitives[0].mode, static_cast<unsigned>(GL_TRIANGLE_STRIP))
        << "plasma.cpp:169 uses a 4-vertex strip, not GL_QUADS";
}

TEST(PlasmaDraw, UploadsTheFieldAndPresentsOncePerFrame) {
    initialiseSaver();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_GE(t.countCalls("glTexSubImage2D"), 1) << "the computed field must reach the texture";
    EXPECT_EQ(t.countCalls("wglSwapLayerBuffers"), 1) << "exactly one present per frame";
}

TEST(PlasmaDraw, StatisticsOverlayKeepsTheMatrixStackBalanced) {
    // The kStatistics branch pushes on both PROJECTION and MODELVIEW and pops
    // them in the reverse order. It is off by default, so nothing else covers it.
    initialiseSaver();
    svStatistics() = 1;
    draw();
    svStatistics() = 0;

    const glstub::Trace& t = glstub::trace();
    EXPECT_TRUE(t.matrixBalanced())
        << "depth " << t.matrixDepth << ", " << t.pushes << " pushes vs " << t.pops << " pops";
    EXPECT_GE(t.pushes, 2) << "the overlay should have pushed both matrix modes";
}

// --- framework entry points ------------------------------------------------

TEST(PlasmaFramework, ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy) {
    svReadyToDraw() = 0;
    glstub::reset();

    screenSaverProc(hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(svReadyToDraw(), 1);
    EXPECT_GE(glstub::trace().texturesGenerated, 1);

    screenSaverProc(hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(svReadyToDraw(), 0);
}

TEST(PlasmaFramework, ScreenSaverProcPassesUnhandledMessagesThrough) {
    // Anything it does not handle must reach defScreenSaverProc rather than
    // being swallowed.
    EXPECT_NO_FATAL_FAILURE(screenSaverProc(hostWindow(), WM_PAINT, 0, 0));
}

TEST(PlasmaSettings, ReadRegistryLeavesEveryValueUsable) {
    // readRegistry only reads: it calls setDefaults first and returns early if
    // the key is absent, so running it cannot disturb the machine.
    //
    // That early return also means this covers little on a machine where the
    // saver has never stored settings, CI included. See the KNOWN LIMITATION
    // note in test_cyclone.cpp. Whatever is
    // actually stored under HKCU, the result must be usable.
    //
    // Note this is the unclamped read that Task 11 targets, so today it only
    // asserts non-degeneracy, not range.
    readRegistry();

    EXPECT_NE(svZoom(), 0) << "dZoom divides into 30.0f in setPlasmaSize";
    EXPECT_GT(svResolution(), 0);
    EXPECT_GT(svSpeed(), 0);
}

// --- dialog procedures -----------------------------------------------------
//
// A dialog procedure is an ordinary message handler, so it can be driven
// directly. With a null HWND, GetDlgItem and SendDlgItemMessage return null or
// fail harmlessly, which is enough to walk the switch arms.
//
// IDOK is deliberately never sent: it calls writeRegistry and would rewrite the
// user's real saver settings.

TEST(PlasmaDialogs, AboutProcColoursTheWebPageLabel) {
    // WM_CTLCOLORSTATIC returns a brush through an INT_PTR - the truncation
    // that PR #39 fixed. Passing lParam 0 matches GetDlgItem's null result, so
    // the branch is taken.
    const INT_PTR brush = aboutProc(nullptr, WM_CTLCOLORSTATIC, 0, 0);
    EXPECT_NE(brush, 0) << "the handler must return a brush, not fall through";
}

TEST(PlasmaDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_EQ(aboutProc(nullptr, WM_MOUSEMOVE, 0, 0), FALSE);
}

TEST(PlasmaDialogs, InitControlsRunsWithoutADialog) {
    setDefaults();
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(PlasmaDialogs, ConfigureDialogInitialisesAndCancels) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_INITDIALOG, 0, 0), TRUE);
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_COMMAND, IDCANCEL, 0), TRUE);
}

TEST(PlasmaDialogs, ConfigureDialogRestoresDefaults) {
    setDefaults();
    const int defaultZoom = svZoom();
    svZoom() = 99;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS, 0);

    EXPECT_EQ(svZoom(), defaultZoom) << "the Defaults button must reset the settings";
}

TEST(PlasmaDialogs, ConfigureDialogHandlesSliderMovement) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_HSCROLL, 0, 0), TRUE);
}

TEST(PlasmaDialogs, ConfigureDialogIgnoresUnknownMessages) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_MOUSEMOVE, 0, 0), FALSE);
}

TEST(PlasmaDraw, IdleProcSkipsDrawingWhenNotReady) {
    initialiseSaver();
    svReadyToDraw() = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().totalVertices(), 0u)
        << "idleProc must not draw before the saver is ready";
    svReadyToDraw() = 1;
}
