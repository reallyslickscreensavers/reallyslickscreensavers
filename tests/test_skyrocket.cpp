/*
 * Tests for the skyrocket saver.
 *
 * skyrocket is compiled into this binary against the recording GL stub and a
 * stubbed OpenAL, so the real draw path runs headless. Shared scaffolding - the
 * fixture and the frame invariants - lives in support/saver_test_common.h; what
 * is here is what is specific to skyrocket.
 *
 * It is the only saver with sound, and the only one that both reads the
 * matrices back for gluProject (skyrocket.cpp:341, for the lens flares) and
 * carries a second subsystem to stub out.
 */

#include "support/saver_test_common.h"

// Safe to include here: skyrocket uses rsMath's generator and carries no private
// copy of it. Seven savers do carry one, which is why the shared header cannot
// include this - see the note on kTestSeed there, and Task 12.
#include <rsMath/rsMath.h>

#include <array>

#include "resource.h"

// skyrocket.cpp has no header; its contract with the framework is by name. See
// the note on cpp:S5421 in test_fieldlines.cpp - these are declarations, not
// definitions.
extern int dMaxrockets;
extern int dSmoke;
extern int dExplosionsmoke;
extern int dWind;
extern int dAmbient;
extern int dStardensity;
extern int dFlare;
extern int dMoonglow;
extern int dMoon;
extern int dClouds;
extern int dEarth;
extern int dIllumination;
extern int dSound;
extern int readyToDraw;

// Seconds since the last frame. idleProc sets it from an rsTimer
// (skyrocket.cpp:910), but the tests call draw() directly, so it stays at its
// initial 0.0f unless a test drives it - and at zero the simulation is frozen.
extern float frameTime;

// How many particles are live. Rockets, explosions, smoke and shockwaves are
// all particles, so this is how a test tells whether anything actually flew.
extern unsigned int last_particle;

void setDefaults();
void readRegistry();
void initControls(HWND hdlg);
LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

// skyrocket spells its teardown cleanup, lowercase u, where the other twelve
// savers and the shared fixture use cleanUp (skyrocket.cpp:964).
void cleanup(HWND hwnd);

void cleanUp(HWND hwnd) { cleanup(hwnd); }

namespace {

// A plausible frame at 30fps, and enough of them for rockets to launch, climb
// and burst. Both numbers are deliberately modest: the point is to reach the
// explosion, smoke and shockwave paths once, not to simulate a display.
constexpr float kFrameSeconds = 1.0f / 30.0f;
constexpr int kSimulatedFrames = 60;

// Bounds the work per frame. Every live rocket becomes hundreds of particles on
// bursting, each of them drawn, and the cost of that is what decides how long
// the instrumented run takes - one unlucky seed with the shipped eight rockets
// turned a forty-minute coverage run into a six-hour one.
//
// The seed is fixed now (see kTestSeed in saver_test_common.h), so this is belt
// and braces: it keeps the worst case affordable if the seed ever changes. Two
// rockets still launch, burst and smoke, which is all the coverage needs.
constexpr int kBoundedRockets = 2;

// Sound off by default. The engine is exercised deliberately in its own case
// below; everywhere else it is noise in the trace.
class Skyrocket : public savertest::SaverFixture {
protected:
    void SetUp() override {
        // Before setDefaults, and before anything reaches initSaver. Without it
        // the rocket and explosion types are drawn from a random_device seed and
        // the run time is unbounded: skyrocket picks a mega-explosion on
        // if(!rsRandi(2500)), and one instrumented run that hit it took 358
        // minutes against a normal 40.
        rsRandGen().seed(savertest::kTestSeed);

        setDefaults();
        dSound = 0;
    }
};

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(SkyrocketHarness, SaverBodyWasActuallyCompiled) {
    // Guards the WIN32-define trap. MSVC predefines _WIN32, not WIN32, and the
    // whole saver sits inside #ifdef WIN32; without it this links against an
    // empty translation unit and every other test passes vacuously.
    setDefaults();
    EXPECT_EQ(dMaxrockets, 8);
    EXPECT_EQ(dSmoke, 10);
    EXPECT_EQ(dWind, 20);
    EXPECT_EQ(dStardensity, 20);
    EXPECT_EQ(dSound, 100);
}

// --- a frame ---------------------------------------------------------------

TEST_F(Skyrocket, FrameLeavesTheMatrixStackBalanced) {
    start();
    draw();
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

TEST_F(Skyrocket, FramePairsBeginAndEnd) {
    start();
    draw();
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

TEST_F(Skyrocket, PrimitiveVertexCountsAreLegal) {
    start();
    draw();
    EXPECT_TRUE(savertest::VertexCountsLegal());
}

TEST_F(Skyrocket, ReadsBackNoInvalidEnums) {
    start();
    draw();
    EXPECT_TRUE(savertest::NoInvalidEnums());
}

TEST_F(Skyrocket, DrawsTheWorldEveryFrame) {
    // The ground, sky and their decorations are all triangle strips and fans
    // (world.cpp:308-469).
    start();
    draw();

    EXPECT_GT(countPrimitives(GL_TRIANGLE_STRIP) + countPrimitives(GL_TRIANGLE_FAN), 0);
    EXPECT_GT(glstub::trace().totalVertices(), 0u);
}

// --- the matrix stack ------------------------------------------------------

TEST_F(Skyrocket, ReadsBackAProjectionItCanProjectWith) {
    // The lens flares call gluProject with whatever glGetDoublev returned
    // (skyrocket.cpp:341) and divide the results by the window size on the next
    // line without checking the return, so the projection has to be real.
    start();
    draw();

    std::array<float, 16> projection{};
    glstub::currentMatrix(GL_PROJECTION, projection.data());

    EXPECT_FLOAT_EQ(projection[11], -1.0f) << "the fourth row must take w from z";
    EXPECT_GT(projection[0], 0.0f);
    EXPECT_GT(projection[5], 0.0f);
    for (int i = 0; i < 16; ++i) {
        EXPECT_TRUE(std::isfinite(projection[i])) << "projection element " << i;
    }
}

TEST_F(Skyrocket, LeavesTheModelviewFiniteAfterAFrame) {
    start();
    draw();

    std::array<float, 16> modelview{};
    glstub::currentMatrix(GL_MODELVIEW, modelview.data());
    for (int i = 0; i < 16; ++i) {
        EXPECT_TRUE(std::isfinite(modelview[i])) << "modelview element " << i;
    }
}

// --- settings change what is drawn -----------------------------------------

// The World compiles everything it draws into display lists in its constructor
// (world.cpp:284-530) and only calls them per frame (world.cpp:638-710). Its
// geometry is therefore visible at setup, and its switches show up in a frame
// as a change in the number of glCallList calls rather than in vertices.
//
// Settings are changed only while nothing is allocated - see Task 16 in
// docs/MAINTENANCE.md.

TEST_F(Skyrocket, WorldFeaturesCanBeTurnedOff) {
    stop();
    dMoon = 0;
    dClouds = 0;
    dEarth = 0;
    dStardensity = 0;
    startCapturingSetup();
    const int bareLists = glstub::trace().countCalls("glNewList");

    stop();
    dMoon = 1;
    dClouds = 1;
    dEarth = 1;
    dStardensity = 20;
    startCapturingSetup();
    const int fullLists = glstub::trace().countCalls("glNewList");

    EXPECT_GT(fullLists, bareLists);
}

TEST_F(Skyrocket, TurningTheWorldOffLeavesLessToCallEachFrame) {
    stop();
    dMoon = 0;
    dClouds = 0;
    dEarth = 0;
    dStardensity = 0;
    start();
    draw();
    const int bare = glstub::trace().countCalls("glCallList");

    stop();
    dMoon = 1;
    dClouds = 1;
    dEarth = 1;
    dStardensity = 20;
    start();
    draw();
    const int full = glstub::trace().countCalls("glCallList");

    EXPECT_GT(full, bare);
}

TEST_F(Skyrocket, StarfieldIsBuiltOnlyWhenAskedFor) {
    // Only the presence of the starfield is assertable here, not its density.
    // dStardensity * 100 stars are painted into a texture bitmap
    // (world.cpp:46-55) which then goes to glTexImage2D, while the sky mesh
    // the texture is drawn on is a fixed STARMESH grid (world.cpp:285-318).
    // The stub sees the mesh, never the pixels.
    stop();
    dStardensity = 0;
    startCapturingSetup();
    const unsigned long long without = glstub::trace().totalVertices();

    stop();
    dStardensity = 20;
    startCapturingSetup();
    const unsigned long long with = glstub::trace().totalVertices();

    EXPECT_GT(with, without);
}

TEST_F(Skyrocket, KeepsDrawingCoherentlyWhileRocketsFly) {
    // One frame catches an empty sky; the interesting states - explosions,
    // smoke, shockwaves - only appear once rockets have launched and burst.
    //
    // Which needs time to pass. draw() spends frameTime rather than measuring
    // it (skyrocket.cpp:693), and only idleProc sets it, so a loop of bare
    // draw() calls redraws one frozen instant. This test used to do exactly
    // that for 120 frames: every one emitted an identical 9,940 vertices, it
    // covered nothing the first frame had not, and it cost nine minutes of the
    // instrumented run. last_particle below is the guard against that
    // returning - it stays at zero if the clock is not running.
    stop();
    dMaxrockets = kBoundedRockets;
    start();
    for (int frame = 0; frame < kSimulatedFrames; ++frame) {
        frameTime = kFrameSeconds;
        draw();
    }

    EXPECT_GT(last_particle, 0u)
        << "no rockets launched - is frameTime being driven?";
    EXPECT_TRUE(savertest::PrimitivesPaired());
    EXPECT_TRUE(savertest::VertexCountsLegal());
    EXPECT_TRUE(savertest::MatrixStackBalanced());
    EXPECT_TRUE(savertest::NoInvalidEnums());
}

// --- sound -----------------------------------------------------------------

TEST_F(Skyrocket, RunsWithoutASoundEngine) {
    // dSound gates the SoundEngine entirely (skyrocket.cpp:955), and zero is
    // what a machine with no audio device ends up at.
    stop();
    dSound = 0;
    start();
    EXPECT_NO_FATAL_FAILURE(draw());
    EXPECT_GT(glstub::trace().totalVertices(), 0u);
}

TEST_F(Skyrocket, DrivesTheSoundEngineWhenAsked) {
    // Against tests/support/al_stub.cpp: the device, context, buffers and
    // sources all report success, so the engine builds and plays as it would
    // on a machine with audio.
    //
    // Far fewer frames than the drawing case above. The particle paths are that
    // test's job and this one duplicates them at full price - the two were 43%
    // of the instrumented run between them. What this needs is a rocket in the
    // air and then a while for it to burst.
    //
    // Driven by the simulation rather than a frame count, because a count is a
    // guess about when the first launch happens and that is not a fixed number:
    // rocketTimer is a static inside draw() and carries between cases in a
    // process, so a count with no margin passes under ctest, which forks per
    // test, and fails when the binary is run directly. A fixed ten did exactly
    // that.
    stop();
    dSound = 100;
    dMaxrockets = kBoundedRockets;
    start();

    int frames = 0;
    while (last_particle == 0u && frames < kSimulatedFrames) {
        frameTime = kFrameSeconds;
        draw();
        ++frames;
    }
    ASSERT_GT(last_particle, 0u) << "nothing launched within " << kSimulatedFrames << " frames";

    // Then long enough for it to climb and burst, which is what the engine hears.
    for (int frame = 0; frame < kSimulatedFrames / 2; ++frame) {
        frameTime = kFrameSeconds;
        draw();
    }

    EXPECT_GT(last_particle, 0u) << "nothing flew, so nothing could be heard";
    EXPECT_TRUE(savertest::PrimitivesPaired());
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

// --- framework entry points ------------------------------------------------

TEST_F(Skyrocket, IdleProcSkipsDrawingWhenNotReady) {
    start();
    readyToDraw = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().totalVertices(), 0u);
    readyToDraw = 1;
}

TEST(SkyrocketFramework, ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy) {
    setDefaults();
    dSound = 0;
    readyToDraw = 0;

    screenSaverProc(testsupport::hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(readyToDraw, 1);

    screenSaverProc(testsupport::hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(readyToDraw, 0);
}

TEST(SkyrocketFramework, ReadRegistryLeavesEveryValueUsable) {
    // Read-only: setDefaults runs first and the function returns early if the
    // key is absent, so this cannot disturb the machine. That early return also
    // means it covers little where the saver has never stored settings, CI
    // included - see the note in test_cyclone.cpp.
    setDefaults();
    readRegistry();

    EXPECT_GT(dMaxrockets, 0) << "the rocket array is allocated with this count";
    EXPECT_GE(dSmoke, 0);
    EXPECT_GE(dStardensity, 0);
    EXPECT_GE(dSound, 0) << "and it scales the gain, so it must not go negative";
}

// --- dialog procedures -----------------------------------------------------

TEST(SkyrocketDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_TRUE(savertest::AboutProcColoursTheWebPageLabel(aboutProc));
}

TEST(SkyrocketDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(aboutProc));
}

TEST(SkyrocketDialogs, InitControlsRunsWithoutADialog) {
    setDefaults();
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(SkyrocketDialogs, ConfigureDialogHandlesTheStandardMessages) {
    EXPECT_TRUE(savertest::ConfigureDialogInitialisesAndCancels(screenSaverConfigureDialog));
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(screenSaverConfigureDialog));
}

TEST(SkyrocketDialogs, ConfigureDialogRestoresDefaults) {
    setDefaults();
    const int defaultRockets = dMaxrockets;
    dMaxrockets = 99;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS, 0);

    EXPECT_EQ(dMaxrockets, defaultRockets);
}
