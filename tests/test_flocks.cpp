/*
 * Tests for the flocks saver.
 *
 * flocks is the stub's stress case: only about a quarter of its function lines
 * avoid OpenGL, and bug::update both advances the simulation and draws it, so
 * there is no seam between the two. Everything here therefore goes through the
 * saver's real entry points and asserts on the recorded command stream.
 */

#include <gtest/gtest.h>

#include <Windows.h>
#include <gl/GL.h>

#include "support/gl_stub.h"
#include "support/test_window.h"
#include "resource.h"

// flocks.cpp has no header; the saver's contract is by name.
//
// The savers keep their settings in mutable globals (docs/MAINTENANCE.md
// Task 6). These accessors reach them without this file declaring globals of
// its own: the extern declarations are block-scope, so they bind to the
// definitions in flocks.cpp. They must sit at global scope - inside a
// namespace a block-scope extern would bind to that namespace instead.
static int& svLeaders()     { extern int dLeaders;     return dLeaders; }
static int& svFollowers()   { extern int dFollowers;   return dFollowers; }
static int& svGeometry()    { extern int dGeometry;    return dGeometry; }
static int& svSize()        { extern int dSize;        return dSize; }
static int& svSpeed()       { extern int dSpeed;       return dSpeed; }
static int& svStretch()     { extern int dStretch;     return dStretch; }
static int& svConnections() { extern int dConnections; return dConnections; }
static int& svReadyToDraw() { extern int readyToDraw;  return readyToDraw; }

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

// initSaver allocates lBugs/fBugs, so every test that calls it must pair with
// cleanUp or leak an array per case.
class Flocks : public ::testing::Test {
protected:
    void SetUp() override { setDefaults(); }
    void TearDown() override {
        if (started_) cleanUp(hostWindow());
    }

    // Brings the saver up and throws the first frame away.
    //
    // flocks' draw() lazily constructs its rsText on the very first call
    // (`static int first`), so a cold frame carries font-building GL calls a
    // warm one does not. Measuring without this made
    // MoreBugsMeansMoreDrawing pass in suite order and fail in isolation,
    // which is exactly how ctest runs it.
    void start() {
        initSaver(hostWindow());
        started_ = true;
        draw();           // warm-up
        glstub::reset();
    }

    // Same, for tests that need to re-init with different settings.
    void restart() {
        cleanUp(hostWindow());
        started_ = false;
        start();
    }
private:
    bool started_ = false;
};

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(FlocksHarness, SaverBodyWasActuallyCompiled) {
    // Guards the WIN32-define trap: without it the saver preprocesses away and
    // every other test would pass against an empty translation unit.
    setDefaults();
    EXPECT_EQ(svLeaders(), 4);
    EXPECT_EQ(svFollowers(), 1000);
    EXPECT_EQ(svSize(), 5);
    EXPECT_EQ(svSpeed(), 15);
}

// --- startup ---------------------------------------------------------------

TEST_F(Flocks, InitSaverEnablesDepthTestingAndLighting) {
    glstub::reset();
    initSaver(hostWindow());

    const glstub::Trace& t = glstub::trace();
    EXPECT_GT(t.netEnable(GL_DEPTH_TEST), 0) << "flocks draws in 3D";
    EXPECT_GT(t.netEnable(GL_LIGHTING), 0) << "dGeometry defaults on, which lights the blobs";
    EXPECT_GE(t.countCalls("glNewList"), 1) << "the bug blob is compiled into a display list";

    cleanUp(hostWindow());
}

TEST_F(Flocks, DotsModeSkipsTheLightingSetup) {
    svGeometry() = 0;
    glstub::reset();
    initSaver(hostWindow());

    EXPECT_EQ(glstub::trace().countCalls("glNewList"), 0)
        << "no display list is needed when bugs are drawn as dots";

    cleanUp(hostWindow());
}

// --- a frame ---------------------------------------------------------------

TEST_F(Flocks, FrameLeavesTheMatrixStackBalanced) {
    start();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_TRUE(t.matrixBalanced())
        << "depth " << t.matrixDepth << ", " << t.pushes << " pushes vs " << t.pops << " pops";
    EXPECT_GE(t.minMatrixDepth, 0) << "popped further than it pushed";
}

TEST_F(Flocks, FramePairsBeginAndEnd) {
    start();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_TRUE(t.primitivesBalanced()) << t.begins << " glBegin vs " << t.ends << " glEnd";
    EXPECT_FALSE(t.nestedBeginSeen);
    EXPECT_FALSE(t.vertexOutsideBegin);
}

TEST_F(Flocks, PrimitiveVertexCountsAreLegal) {
    start();
    draw();

    std::string why;
    EXPECT_TRUE(glstub::primitiveVertexCountsLegal(&why)) << why;
}

// --- the settings actually change what is drawn ----------------------------

TEST_F(Flocks, GeometryModeIssuesOneDisplayListCallPerBug) {
    svGeometry() = 1;
    svLeaders() = 3;
    svFollowers() = 20;
    start();
    draw();

    EXPECT_EQ(glstub::trace().countCalls("glCallList"), svLeaders() + svFollowers())
        << "each bug draws itself once from the compiled blob";
}

TEST_F(Flocks, DotsModeEmitsOnePointPerBug) {
    svGeometry() = 0;
    svStretch() = 0;
    svLeaders() = 2;
    svFollowers() = 15;
    start();
    draw();

    EXPECT_EQ(countPrimitives(GL_POINTS), svLeaders() + svFollowers());
    EXPECT_EQ(countPrimitives(GL_LINES), 0) << "unstretched dots are points, not lines";
}

TEST_F(Flocks, StretchTurnsDotsIntoLines) {
    svGeometry() = 0;
    svStretch() = 20;
    svConnections() = 0;
    svLeaders() = 2;
    svFollowers() = 15;
    start();
    draw();

    EXPECT_EQ(countPrimitives(GL_LINES), svLeaders() + svFollowers())
        << "a stretched bug is drawn as a motion-blur line";
    EXPECT_EQ(countPrimitives(GL_POINTS), 0);
}

TEST_F(Flocks, ConnectionsDrawAnExtraLinePerFollower) {
    svGeometry() = 0;
    svStretch() = 0;
    svLeaders() = 2;
    svFollowers() = 15;

    svConnections() = 0;
    start();
    draw();
    const int withoutConnections = countPrimitives(GL_LINES);

    svConnections() = 1;
    glstub::reset();
    draw();
    const int withConnections = countPrimitives(GL_LINES);

    EXPECT_EQ(withoutConnections, 0);
    EXPECT_EQ(withConnections, svFollowers())
        << "each follower draws a line back to its leader; leaders have none";
}

TEST_F(Flocks, MoreBugsMeansMoreDrawing) {
    svGeometry() = 0;
    svStretch() = 0;
    svLeaders() = 2;
    svFollowers() = 10;
    start();
    draw();
    const unsigned long long few = glstub::trace().totalVertices();

    svFollowers() = 100;
    restart();
    draw();
    const unsigned long long many = glstub::trace().totalVertices();

    EXPECT_GT(few, 0u);
    EXPECT_GT(many, few) << "flocks draws per bug, unlike plasma's fixed quad";
}

// --- framework entry points ------------------------------------------------

TEST_F(Flocks, IdleProcSkipsDrawingWhenNotReady) {
    start();
    svReadyToDraw() = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().totalVertices(), 0u);
    EXPECT_EQ(glstub::trace().countCalls("glCallList"), 0);
    svReadyToDraw() = 1;
}

TEST(FlocksFramework, ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy) {
    setDefaults();
    svReadyToDraw() = 0;

    screenSaverProc(hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(svReadyToDraw(), 1);

    screenSaverProc(hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(svReadyToDraw(), 0);
}

TEST(FlocksFramework, ReadRegistryLeavesEveryValueUsable) {
    // Read-only: setDefaults runs first and the function returns early if the
    // key is absent, so this cannot disturb the machine.
    //
    // That early return also means this covers little on a machine where the
    // saver has never stored settings, CI included. See the KNOWN LIMITATION
    // note in test_cyclone.cpp.
    readRegistry();

    EXPECT_GT(svLeaders(), 0) << "lBugs is allocated with this count";
    EXPECT_GT(svFollowers(), 0) << "fBugs is allocated with this count";
    EXPECT_GT(svSize(), 0);
}

// --- dialog procedures -----------------------------------------------------
//
// IDOK is never sent: it calls writeRegistry and would rewrite the user's real
// saver settings.

TEST(FlocksDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_NE(aboutProc(nullptr, WM_CTLCOLORSTATIC, 0, 0), 0);
}

TEST(FlocksDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_EQ(aboutProc(nullptr, WM_MOUSEMOVE, 0, 0), FALSE);
}

TEST(FlocksDialogs, InitControlsRunsWithoutADialog) {
    setDefaults();
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(FlocksDialogs, ConfigureDialogInitialisesAndCancels) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_INITDIALOG, 0, 0), TRUE);
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_COMMAND, IDCANCEL, 0), TRUE);
}

TEST(FlocksDialogs, ConfigureDialogRestoresDefaults) {
    setDefaults();
    const int defaultFollowers = svFollowers();
    svFollowers() = 7;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS, 0);

    EXPECT_EQ(svFollowers(), defaultFollowers);
}

TEST(FlocksDialogs, ConfigureDialogHandlesSliderMovement) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_HSCROLL, 0, 0), TRUE);
}

TEST(FlocksDialogs, ConfigureDialogIgnoresUnknownMessages) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_MOUSEMOVE, 0, 0), FALSE);
}
