/*
 * Tests for the flocks saver.
 *
 * flocks is the stub's stress case: only about a quarter of its function lines
 * avoid OpenGL, and bug::update both advances the simulation and draws it, so
 * there is no seam between the two. Everything here therefore goes through the
 * saver's real entry points and asserts on the recorded command stream. Shared
 * scaffolding lives in support/saver_test_common.h.
 */

#include "support/saver_test_common.h"


#include "resource.h"

// flocks.cpp has no header; its contract with the framework is by name.
// SonarCloud cpp:S5421 flags these as mutable globals; they are declarations of
// the saver's own, which is Task 6 in docs/MAINTENANCE.md.
extern int dLeaders;
extern int dFollowers;
extern int dGeometry;
extern int dSize;
extern int dSpeed;
extern int dStretch;
extern int dConnections;
extern int readyToDraw;

void setDefaults();
void readRegistry();
void initControls(HWND hdlg);
LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

namespace {

class Flocks : public savertest::SaverFixture {
protected:
    void SetUp() override {
        rsRandGen().seed(savertest::kTestSeed);
        setDefaults();
    }
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
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

TEST_F(Flocks, FramePairsBeginAndEnd) {
    start();
    draw();
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

TEST_F(Flocks, PrimitiveVertexCountsAreLegal) {
    start();
    draw();
    EXPECT_TRUE(savertest::VertexCountsLegal());
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

    stop();                 // change counts only while nothing is allocated
    dFollowers = 100;
    start();
    draw();
    const unsigned long long many = glstub::trace().totalVertices();

    EXPECT_GT(few, 0u);
    EXPECT_GT(many, few) << "flocks draws per bug, unlike plasma's fixed strip";
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

    screenSaverProc(testsupport::hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(readyToDraw, 1);

    screenSaverProc(testsupport::hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(readyToDraw, 0);
}

TEST(FlocksFramework, ReadRegistryLeavesEveryValueUsable) {
    // Read-only: setDefaults runs first and the function returns early if the
    // key is absent, so this cannot disturb the machine. That early return also
    // means it covers little where the saver has never stored settings, CI
    // included - see the note in test_cyclone.cpp.
    readRegistry();

    EXPECT_GT(dLeaders, 0) << "lBugs is allocated with this count";
    EXPECT_GT(dFollowers, 0) << "fBugs is allocated with this count";
    EXPECT_GT(dSize, 0);
}

// --- dialog procedures -----------------------------------------------------

TEST(FlocksDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_TRUE(savertest::AboutProcColoursTheWebPageLabel(aboutProc));
}

TEST(FlocksDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(aboutProc));
}

TEST(FlocksDialogs, InitControlsRunsWithoutADialog) {
    setDefaults();
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(FlocksDialogs, ConfigureDialogHandlesTheStandardMessages) {
    EXPECT_TRUE(savertest::ConfigureDialogInitialisesAndCancels(screenSaverConfigureDialog));
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(screenSaverConfigureDialog));
}

TEST(FlocksDialogs, ConfigureDialogRestoresDefaults) {
    setDefaults();
    const int defaultFollowers = dFollowers;
    dFollowers = 7;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS, 0);

    EXPECT_EQ(dFollowers, defaultFollowers);
}
