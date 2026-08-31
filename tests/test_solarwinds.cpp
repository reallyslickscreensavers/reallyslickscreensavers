/*
 * Tests for the solarWinds saver.
 *
 * The one saver whose setDefaults takes an argument: six named presets, each
 * worth checking. Shared scaffolding lives in support/saver_test_common.h.
 */

#include "support/saver_test_common.h"

#include <array>

#include "resource.h"
#include "solarWindsSettings.h"
// The saver's module state, reached through its accessor rather than through
// externs of our own (Task 6 in docs/MAINTENANCE.md).
#include "solarWindsState.h"

using solarWindsState::state;

// solarWinds.cpp has no header; its contract with the framework is by name.
void setDefaults(int which);
void readRegistry();
void initControls(HWND hdlg);
LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

namespace {

// stop() before changing any count, never after - see SaverFixture. solarWinds
// is the saver that makes this compulsory: wind::~wind frees particles[i] for
// i < dParticles, the global rather than the count it was constructed with, so
// raising dParticles and then calling cleanUp walks off the end of the array
// and blocks inside the heap. The saver cannot reach that itself, because
// settings only change through the dialog, which writes the registry.
class SolarWinds : public savertest::SaverFixture {
protected:
    void SetUp() override {
        rsRandGen().seed(savertest::kTestSeed);
        setDefaults(DEFAULTS1);
    }
};

// The settings this saver reads and the ranges its own header declares.
// Built once: both cases below assert against the same list, and writing it
// out twice is what tripped the duplication gate.
std::vector<savertest::RangedSetting> declaredRanges() {
    return {
        savertest::Ranged("dWinds", state().dWinds, solarWindsSettings::kWinds),
        savertest::Ranged("dEmitters", state().dEmitters, solarWindsSettings::kEmitters),
        savertest::Ranged("dParticles", state().dParticles, solarWindsSettings::kParticles),
        savertest::Ranged("dGeometry", state().dGeometry, solarWindsSettings::kGeometry),
        savertest::Ranged("dSize", state().dSize, solarWindsSettings::kSize),
        savertest::Ranged("dWindspeed", state().dWindspeed, solarWindsSettings::kWindspeed),
        savertest::Ranged("dEmitterspeed", state().dEmitterspeed, solarWindsSettings::kEmitterspeed),
        savertest::Ranged("dParticlespeed", state().dParticlespeed, solarWindsSettings::kParticlespeed),
        savertest::Ranged("dBlur", state().dBlur, solarWindsSettings::kBlur),
    };
}

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(SolarWindsHarness, SaverBodyWasActuallyCompiled) {
    // Guards the WIN32-define trap: without it the saver preprocesses away and
    // every other test passes against an empty translation unit.
    setDefaults(DEFAULTS1);
    EXPECT_EQ(state().dWinds, 1);
    EXPECT_EQ(state().dEmitters, 30);
    EXPECT_EQ(state().dParticles, 2000);
    EXPECT_EQ(state().dSize, 50);
}

// --- the six presets -------------------------------------------------------

TEST(SolarWindsPresets, EachPresetProducesUsableSettings) {
    // Every preset must leave values the allocator and the draw loop can use;
    // a zero here would mean an empty simulation or a division by zero.
    const std::array<int, 6> presets{DEFAULTS1, DEFAULTS2, DEFAULTS3,
                                     DEFAULTS4, DEFAULTS5, DEFAULTS6};
    for (int preset : presets) {
        setDefaults(preset);
        EXPECT_GT(state().dWinds, 0) << "preset " << preset;
        EXPECT_GT(state().dEmitters, 0) << "preset " << preset;
        EXPECT_GT(state().dParticles, 0) << "preset " << preset;
        EXPECT_GT(state().dSize, 0) << "preset " << preset;
        EXPECT_GE(state().dGeometry, 0) << "preset " << preset;
        EXPECT_LE(state().dGeometry, 2) << "preset " << preset;
        EXPECT_TRUE(savertest::SettingsWithinDeclaredRanges(declaredRanges())) << "preset " << preset;
    }
}

TEST(SolarWindsPresets, PresetsAreDistinct) {
    setDefaults(DEFAULTS1);
    const int regularEmitters = state().dEmitters;
    setDefaults(DEFAULTS3);   // Cold Pricklies
    EXPECT_NE(state().dEmitters, regularEmitters) << "the presets would be pointless if they matched";
}

// --- a frame ---------------------------------------------------------------

TEST_F(SolarWinds, FrameLeavesTheMatrixStackBalanced) {
    start();
    draw();
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

TEST_F(SolarWinds, FramePairsBeginAndEnd) {
    start();
    draw();
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

TEST_F(SolarWinds, PrimitiveVertexCountsAreLegal) {
    start();
    draw();
    EXPECT_TRUE(savertest::VertexCountsLegal());
}

TEST_F(SolarWinds, DoesNotLeakEnableState) {
    start();
    draw();
    EXPECT_TRUE(savertest::NoEnableStateLeaked());
}

// --- the geometry setting picks the primitive ------------------------------

TEST_F(SolarWinds, LightGeometryDrawsFromTheDisplayList) {
    // dGeometry has three modes and only the middle one is immediate-mode
    // points: 0 is "lights", drawn by calling a compiled display list once per
    // particle, 1 is points, 2 is linked lines (solarWinds.cpp:240).
    state().dGeometry = 0;
    state().dEmitters = 4;
    state().dParticles = 40;
    start();
    draw();

    EXPECT_EQ(glstub::trace().countCalls("glCallList"), state().dParticles)
        << "one display list call per particle";
    EXPECT_EQ(countPrimitives(GL_POINTS), 0) << "geometry 0 is not the point mode";
}

TEST_F(SolarWinds, PointGeometryDrawsPoints) {
    state().dGeometry = 1;
    state().dEmitters = 4;
    state().dParticles = 40;
    start();
    draw();

    EXPECT_GT(countPrimitives(GL_POINTS), 0) << "geometry 1 is the point mode";
}

TEST_F(SolarWinds, LineGeometryLinksParticlesIntoLines) {
    // Mode 2 allocates a line list and threads the particles onto it, which is
    // a whole branch of both initSaver and draw that the other two modes never
    // touch.
    state().dGeometry = 2;
    state().dEmitters = 4;
    state().dParticles = 40;
    start();
    draw();

    EXPECT_EQ(countPrimitives(GL_POINTS), 0) << "geometry 2 links particles rather than dotting them";
    EXPECT_GT(countPrimitives(GL_LINES) + countPrimitives(GL_TRIANGLE_STRIP), 0);
    EXPECT_TRUE(savertest::VertexCountsLegal());
}

TEST_F(SolarWinds, DestructorFreesWhatItAllocatedNotCurrentGlobals) {
    // Task 16: the destructor must free by the counts wind was constructed
    // with, not by whatever the globals say now. Exercise dGeometry == 2 too,
    // since that branch (linelist/lastparticle) has its own guard.
    state().dGeometry = 2;
    state().dEmitters = 4;
    state().dParticles = 20;
    start();

    state().dEmitters = 40;
    state().dParticles = 2;
    state().dGeometry = 0;

    EXPECT_NO_FATAL_FAILURE(stop());
}

TEST_F(SolarWinds, MoreParticlesMeansMoreDrawing) {
    state().dGeometry = 1;          // point mode, so particles become vertices
    state().dEmitters = 4;
    state().dParticles = 40;
    start();
    draw();
    const unsigned long long few = glstub::trace().totalVertices();

    stop();                 // never change a count while allocations are live
    state().dParticles = 400;
    start();
    draw();
    const unsigned long long many = glstub::trace().totalVertices();

    EXPECT_GT(few, 0u);
    EXPECT_GT(many, few);
}

// --- framework entry points ------------------------------------------------

TEST_F(SolarWinds, IdleProcSkipsDrawingWhenNotReady) {
    start();
    state().readyToDraw = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().totalVertices(), 0u);
    state().readyToDraw = 1;
}

TEST(SolarWindsFramework, ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy) {
    // The destroy half is the only coverage of that arm: SaverFixture::stop()
    // calls cleanUp directly, so no TEST_F case in this file goes through
    // screenSaverProc. It also guards a fixed defect - solarWinds used to set
    // readyToDraw = 1 here and then free the particle, emitter and wind arrays,
    // leaving idleProc, which guards drawing on exactly this flag, free to draw
    // from freed memory.
    setDefaults(DEFAULTS1);
    state().readyToDraw = 0;

    screenSaverProc(testsupport::hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(state().readyToDraw, 1);

    screenSaverProc(testsupport::hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(state().readyToDraw, 0);
}

TEST(SolarWindsFramework, ReadRegistryFallsBackToTheRegularPreset) {
    // readRegistry calls setDefaults(DEFAULTS1) before touching the registry,
    // so whatever is stored, the result must be usable. Read-only, and it
    // returns early if the key is absent - see the note in test_cyclone.cpp.
    readRegistry();

    EXPECT_GT(state().dEmitters, 0) << "the emitter array is allocated with this count";
    EXPECT_GT(state().dParticles, 0);
    EXPECT_GT(state().dWinds, 0);
}

TEST(SolarWindsFramework, ReadRegistryLeavesEverySettingInsideItsDeclaredRange) {
    // Read-only: setDefaults(DEFAULTS1) runs first and the function returns
    // early if the key is absent, so this cannot disturb the machine. That
    // early return also means the clamp itself covers little where the saver
    // has never stored settings, CI included - see the note in
    // test_cyclone.cpp.
    readRegistry();

    EXPECT_TRUE(savertest::SettingsWithinDeclaredRanges(declaredRanges()));
}

// --- dialog procedures -----------------------------------------------------

TEST(SolarWindsDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_TRUE(savertest::AboutProcColoursTheWebPageLabel(aboutProc));
}

TEST(SolarWindsDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(aboutProc));
}

TEST(SolarWindsDialogs, InitControlsRunsWithoutADialog) {
    setDefaults(DEFAULTS1);
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(SolarWindsDialogs, ConfigureDialogHandlesTheStandardMessages) {
    EXPECT_TRUE(savertest::ConfigureDialogInitialisesAndCancels(screenSaverConfigureDialog));
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(screenSaverConfigureDialog));
}

TEST(SolarWindsDialogs, EachPresetButtonAppliesItsPreset) {
    setDefaults(DEFAULTS1);
    const int regularEmitters = state().dEmitters;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS3, 0);
    EXPECT_NE(state().dEmitters, regularEmitters) << "the Cold Pricklies button must change the settings";

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS1, 0);
    EXPECT_EQ(state().dEmitters, regularEmitters) << "and Regular must put them back";
}
