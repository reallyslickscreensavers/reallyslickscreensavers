/*
 * Tests for the flocks saver.
 *
 * flocks is the stub's stress case: only about a quarter of its function lines
 * avoid OpenGL, and bug::update both advances the simulation and draws it, so
 * there is no seam between the two. Everything here therefore goes through the
 * saver's real entry points and asserts on the recorded command stream.
 */

#include <gtest/gtest.h>

#include <windows.h>
#include <GL/gl.h>

#include "support/gl_stub.h"
#include "support/test_window.h"
#include "resource.h"

// flocks.cpp has no header; the saver's contract is by name.
extern int dLeaders;
extern int dFollowers;
extern int dGeometry;
extern int dSize;
extern int dComplexity;
extern int dSpeed;
extern int dStretch;
extern int dColorfadespeed;
extern int dChromatek;
extern int dConnections;
extern int readyToDraw;
extern float aspectRatio;

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
    EXPECT_EQ(dLeaders, 4);
    EXPECT_EQ(dFollowers, 1000);
    EXPECT_EQ(dSize, 5);
    EXPECT_EQ(dSpeed, 15);
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
    dGeometry = 0;
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
    dGeometry = 1;
    dLeaders = 3;
    dFollowers = 20;
    start();
    draw();

    EXPECT_EQ(glstub::trace().countCalls("glCallList"), dLeaders + dFollowers)
        << "each bug draws itself once from the compiled blob";
}

TEST_F(Flocks, DotsModeEmitsOnePointPerBug) {
    dGeometry = 0;
    dStretch = 0;
    dLeaders = 2;
    dFollowers = 15;
    start();
    draw();

    EXPECT_EQ(countPrimitives(GL_POINTS), dLeaders + dFollowers);
    EXPECT_EQ(countPrimitives(GL_LINES), 0) << "unstretched dots are points, not lines";
}

TEST_F(Flocks, StretchTurnsDotsIntoLines) {
    dGeometry = 0;
    dStretch = 20;
    dConnections = 0;
    dLeaders = 2;
    dFollowers = 15;
    start();
    draw();

    EXPECT_EQ(countPrimitives(GL_LINES), dLeaders + dFollowers)
        << "a stretched bug is drawn as a motion-blur line";
    EXPECT_EQ(countPrimitives(GL_POINTS), 0);
}

TEST_F(Flocks, ConnectionsDrawAnExtraLinePerFollower) {
    dGeometry = 0;
    dStretch = 0;
    dLeaders = 2;
    dFollowers = 15;

    dConnections = 0;
    start();
    draw();
    const int withoutConnections = countPrimitives(GL_LINES);

    dConnections = 1;
    glstub::reset();
    draw();
    const int withConnections = countPrimitives(GL_LINES);

    EXPECT_EQ(withoutConnections, 0);
    EXPECT_EQ(withConnections, dFollowers)
        << "each follower draws a line back to its leader; leaders have none";
}

TEST_F(Flocks, MoreBugsMeansMoreDrawing) {
    dGeometry = 0;
    dStretch = 0;
    dLeaders = 2;
    dFollowers = 10;
    start();
    draw();
    const unsigned long long few = glstub::trace().totalVertices();

    dFollowers = 100;
    restart();
    draw();
    const unsigned long long many = glstub::trace().totalVertices();

    EXPECT_GT(few, 0u);
    EXPECT_GT(many, few) << "flocks draws per bug, unlike plasma's fixed quad";
}

// --- framework entry points ------------------------------------------------

TEST_F(Flocks, IdleProcSkipsDrawingWhenNotReady) {
    start();
    readyToDraw = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().totalVertices(), 0u);
    EXPECT_EQ(glstub::trace().countCalls("glCallList"), 0);
    readyToDraw = 1;
}

TEST(FlocksFramework, ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy) {
    setDefaults();
    readyToDraw = 0;

    screenSaverProc(hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(readyToDraw, 1);

    screenSaverProc(hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(readyToDraw, 0);
}

TEST(FlocksFramework, ReadRegistryLeavesEveryValueUsable) {
    // Read-only: setDefaults runs first and the function returns early if the
    // key is absent, so this cannot disturb the machine.
    readRegistry();

    EXPECT_GT(dLeaders, 0) << "lBugs is allocated with this count";
    EXPECT_GT(dFollowers, 0) << "fBugs is allocated with this count";
    EXPECT_GT(dSize, 0);
}

// --- dialog procedures -----------------------------------------------------
//
// IDOK is never sent: it calls writeRegistry and would rewrite the user's real
// saver settings.

TEST(FlocksDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_NE(aboutProc(NULL, WM_CTLCOLORSTATIC, 0, 0), 0);
}

TEST(FlocksDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_EQ(aboutProc(NULL, WM_MOUSEMOVE, 0, 0), FALSE);
}

TEST(FlocksDialogs, InitControlsRunsWithoutADialog) {
    setDefaults();
    EXPECT_NO_FATAL_FAILURE(initControls(NULL));
}

TEST(FlocksDialogs, ConfigureDialogInitialisesAndCancels) {
    EXPECT_EQ(screenSaverConfigureDialog(NULL, WM_INITDIALOG, 0, 0), TRUE);
    EXPECT_EQ(screenSaverConfigureDialog(NULL, WM_COMMAND, IDCANCEL, 0), TRUE);
}

TEST(FlocksDialogs, ConfigureDialogRestoresDefaults) {
    setDefaults();
    const int defaultFollowers = dFollowers;
    dFollowers = 7;

    screenSaverConfigureDialog(NULL, WM_COMMAND, DEFAULTS, 0);

    EXPECT_EQ(dFollowers, defaultFollowers);
}

TEST(FlocksDialogs, ConfigureDialogHandlesSliderMovement) {
    EXPECT_EQ(screenSaverConfigureDialog(NULL, WM_HSCROLL, 0, 0), TRUE);
}

TEST(FlocksDialogs, ConfigureDialogIgnoresUnknownMessages) {
    EXPECT_EQ(screenSaverConfigureDialog(NULL, WM_MOUSEMOVE, 0, 0), FALSE);
}
