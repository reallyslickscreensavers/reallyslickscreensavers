/*
 * Tests for the solarWinds saver.
 *
 * solarWinds.cpp is compiled into this binary against the recording GL stub, so
 * the real draw path runs headless. It is the one saver whose setDefaults takes
 * an argument: six named presets, each of which is worth checking.
 */

#include <gtest/gtest.h>

#include <Windows.h>
#include <gl/GL.h>

#include "support/gl_stub.h"
#include "support/test_window.h"
#include "resource.h"

// solarWinds.cpp has no header; its contract with the framework is by name.
//
// SonarCloud cpp:S5421 flags these as mutable globals. They are declarations,
// not definitions - the variables live in solarWinds.cpp - but the rule cannot
// tell the difference. The finding is really about the savers exposing their
// settings as mutable globals at all, which is Task 6 in docs/MAINTENANCE.md.
extern int dWinds;
extern int dEmitters;
extern int dParticles;
extern int dGeometry;
extern int dSize;
extern int dParticlespeed;
extern int dEmitterspeed;
extern int dWindspeed;
extern int dBlur;
extern int readyToDraw;

void setDefaults(int which);
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

class SolarWinds : public ::testing::Test {
protected:
    void SetUp() override { setDefaults(DEFAULTS1); }
    void TearDown() override { if (started_) cleanUp(hostWindow()); }

    // Discards the first frame so every test measures a warm one; ctest runs
    // each case in its own process.
    void start() {
        initSaver(hostWindow());
        started_ = true;
        draw();           // warm-up
        glstub::reset();
    }

    // Settings must only change while nothing is allocated, so stopping is a
    // separate step rather than part of a restart helper.
    //
    // wind::~wind frees particles[i] for i < dParticles - the global, not the
    // count it was constructed with (solarWinds.cpp, the destructor next to the
    // constructor). Raise dParticles and then call cleanUp and it walks off the
    // end of the array deleting garbage pointers; the process blocks inside the
    // heap and never returns. That is what it did here for ten minutes at zero
    // CPU before it was understood.
    //
    // The saver never hits this itself: settings only change through the dialog,
    // which writes the registry, and initSaver/cleanUp bracket a whole run. It
    // is a trap for tests, not a live defect - though a destructor that
    // remembered its own size would be better.
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

TEST(SolarWindsHarness, SaverBodyWasActuallyCompiled) {
    // Guards the WIN32-define trap: without it the saver preprocesses away and
    // every other test passes against an empty translation unit.
    setDefaults(DEFAULTS1);
    EXPECT_EQ(dWinds, 1);
    EXPECT_EQ(dEmitters, 30);
    EXPECT_EQ(dParticles, 2000);
    EXPECT_EQ(dSize, 50);
}

// --- the six presets -------------------------------------------------------

TEST(SolarWindsPresets, EachPresetProducesUsableSettings) {
    // Every preset must leave values the allocator and the draw loop can use;
    // a zero here would mean an empty simulation or a division by zero.
    const int presets[] = {DEFAULTS1, DEFAULTS2, DEFAULTS3, DEFAULTS4, DEFAULTS5, DEFAULTS6};
    for (int preset : presets) {
        setDefaults(preset);
        EXPECT_GT(dWinds, 0) << "preset " << preset;
        EXPECT_GT(dEmitters, 0) << "preset " << preset;
        EXPECT_GT(dParticles, 0) << "preset " << preset;
        EXPECT_GT(dSize, 0) << "preset " << preset;
        EXPECT_GE(dGeometry, 0) << "preset " << preset;
        EXPECT_LE(dGeometry, 2) << "preset " << preset;
    }
}

TEST(SolarWindsPresets, PresetsAreDistinct) {
    setDefaults(DEFAULTS1);
    const int regularEmitters = dEmitters;
    setDefaults(DEFAULTS3);   // Cold Pricklies
    EXPECT_NE(dEmitters, regularEmitters) << "the presets would be pointless if they matched";
}

// --- a frame ---------------------------------------------------------------

TEST_F(SolarWinds, FrameLeavesTheMatrixStackBalanced) {
    start();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_TRUE(t.matrixBalanced())
        << "depth " << t.matrixDepth << ", " << t.pushes << " pushes vs " << t.pops << " pops";
    EXPECT_GE(t.minMatrixDepth, 0);
}

TEST_F(SolarWinds, FramePairsBeginAndEnd) {
    start();
    draw();

    const glstub::Trace& t = glstub::trace();
    EXPECT_TRUE(t.primitivesBalanced()) << t.begins << " glBegin vs " << t.ends << " glEnd";
    EXPECT_FALSE(t.nestedBeginSeen);
    EXPECT_FALSE(t.vertexOutsideBegin);
}

TEST_F(SolarWinds, PrimitiveVertexCountsAreLegal) {
    start();
    draw();

    std::string why;
    EXPECT_TRUE(glstub::primitiveVertexCountsLegal(&why)) << why;
}

TEST_F(SolarWinds, FrameEmitsGeometry) {
    start();
    draw();
    EXPECT_GT(glstub::trace().totalVertices(), 0u);
}

TEST_F(SolarWinds, DoesNotLeakEnableState) {
    start();
    draw();
    for (const auto& [capability, net] : glstub::trace().enables) {
        EXPECT_EQ(net, 0) << "capability " << capability << " left with net enable " << net;
    }
}

// --- the geometry setting picks the primitive ------------------------------

TEST_F(SolarWinds, LightGeometryDrawsFromTheDisplayList) {
    // dGeometry has three modes and only the middle one is immediate-mode
    // points: 0 is "lights", drawn by calling a compiled display list once per
    // particle, 1 is points, 2 is linked lines (solarWinds.cpp:240).
    setDefaults(DEFAULTS1);
    dGeometry = 0;
    dEmitters = 4;
    dParticles = 40;
    start();
    draw();

    EXPECT_EQ(glstub::trace().countCalls("glCallList"), dParticles)
        << "one display list call per particle";
    EXPECT_EQ(countPrimitives(GL_POINTS), 0) << "geometry 0 is not the point mode";
}

TEST_F(SolarWinds, PointGeometryDrawsPoints) {
    setDefaults(DEFAULTS1);
    dGeometry = 1;
    dEmitters = 4;
    dParticles = 40;
    start();
    draw();

    EXPECT_GT(countPrimitives(GL_POINTS), 0) << "geometry 1 is the point mode";
}

TEST_F(SolarWinds, LineGeometryDrawsLines) {
    setDefaults(DEFAULTS1);
    dGeometry = 2;
    dEmitters = 4;
    dParticles = 40;
    start();
    draw();

    EXPECT_EQ(countPrimitives(GL_POINTS), 0) << "geometry 2 links particles into lines";
    EXPECT_GT(countPrimitives(GL_LINES) + countPrimitives(GL_TRIANGLE_STRIP), 0);
}

TEST_F(SolarWinds, MoreParticlesMeansMoreDrawing) {
    setDefaults(DEFAULTS1);
    dGeometry = 1;          // point mode, so particles become vertices
    dEmitters = 4;
    dParticles = 40;
    start();
    draw();
    const unsigned long long few = glstub::trace().totalVertices();

    // stop before changing dParticles, never after - see stop() for why.
    stop();
    dParticles = 400;
    start();
    draw();
    const unsigned long long many = glstub::trace().totalVertices();

    EXPECT_GT(few, 0u);
    EXPECT_GT(many, few);
}

// --- framework entry points ------------------------------------------------

TEST_F(SolarWinds, IdleProcSkipsDrawingWhenNotReady) {
    start();
    readyToDraw = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().totalVertices(), 0u);
    readyToDraw = 1;
}

TEST(SolarWindsFramework, ScreenSaverProcInitialisesOnCreate) {
    setDefaults(DEFAULTS1);
    readyToDraw = 0;

    screenSaverProc(hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(readyToDraw, 1);

    screenSaverProc(hostWindow(), WM_DESTROY, 0, 0);
}

TEST(SolarWindsFramework, DestroyLeavesReadyToDrawSet) {
    // DEFECT, pinned rather than fixed - this test documents current behaviour.
    //
    // solarWinds.cpp:858 sets readyToDraw = 1 in the WM_DESTROY handler, then
    // calls cleanUp, which deletes the particle, emitter and wind arrays. Every
    // other saver sets it to 0 there, and idleProc guards drawing on exactly
    // this flag - so a frame arriving after WM_DESTROY would draw from freed
    // memory.
    //
    // It has survived because WM_DESTROY is normally followed straight away by
    // the message loop ending and the process exiting, so nothing gets a chance
    // to call idleProc. That makes it latent rather than harmless: the guard
    // does not do what the identical line in the other twelve savers does.
    //
    // Changing it is a one-character behaviour fix and belongs with the
    // reliability work in docs/MAINTENANCE.md, not in a test-only change.
    setDefaults(DEFAULTS1);
    screenSaverProc(hostWindow(), WM_CREATE, 0, 0);

    screenSaverProc(hostWindow(), WM_DESTROY, 0, 0);

    EXPECT_EQ(readyToDraw, 1)
        << "if this now fails the defect is fixed - fold this back into the create/destroy test";
    readyToDraw = 0;
}

TEST(SolarWindsFramework, ReadRegistryFallsBackToTheRegularPreset) {
    // readRegistry calls setDefaults(DEFAULTS1) before touching the registry,
    // so whatever is stored, the result must be usable. Read-only, and it
    // returns early if the key is absent - see the note in test_cyclone.cpp.
    readRegistry();

    EXPECT_GT(dEmitters, 0) << "the emitter array is allocated with this count";
    EXPECT_GT(dParticles, 0);
    EXPECT_GT(dWinds, 0);
}

// --- dialog procedures -----------------------------------------------------
//
// IDOK is never sent: it calls writeRegistry and would rewrite real settings.

TEST(SolarWindsDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_NE(aboutProc(nullptr, WM_CTLCOLORSTATIC, 0, 0), 0);
}

TEST(SolarWindsDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_EQ(aboutProc(nullptr, WM_MOUSEMOVE, 0, 0), FALSE);
}

TEST(SolarWindsDialogs, InitControlsRunsWithoutADialog) {
    setDefaults(DEFAULTS1);
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(SolarWindsDialogs, ConfigureDialogInitialisesAndCancels) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_INITDIALOG, 0, 0), TRUE);
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_COMMAND, IDCANCEL, 0), TRUE);
}

TEST(SolarWindsDialogs, EachPresetButtonAppliesItsPreset) {
    setDefaults(DEFAULTS1);
    const int regularEmitters = dEmitters;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS3, 0);
    EXPECT_NE(dEmitters, regularEmitters) << "the Cold Pricklies button must change the settings";

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS1, 0);
    EXPECT_EQ(dEmitters, regularEmitters) << "and Regular must put them back";
}

TEST(SolarWindsDialogs, ConfigureDialogHandlesSliderMovement) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_HSCROLL, 0, 0), TRUE);
}

TEST(SolarWindsDialogs, ConfigureDialogIgnoresUnknownMessages) {
    EXPECT_EQ(screenSaverConfigureDialog(nullptr, WM_MOUSEMOVE, 0, 0), FALSE);
}
