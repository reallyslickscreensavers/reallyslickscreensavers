/*
 * Tests for the flux saver.
 *
 * flux.cpp is compiled into this binary against the recording GL stub, so the
 * real draw path runs headless. Shared scaffolding - the fixture and the frame
 * invariants - lives in support/saver_test_common.h; what is here is what is
 * specific to flux.
 */

#include "support/saver_test_common.h"

#include <array>

#include "resource.h"

// flux.cpp has no header; its contract with the framework is by name. See the
// note on cpp:S5421 in test_fieldlines.cpp - these are declarations, not
// definitions.
extern int dFluxes;
extern int dParticles;
extern int dTrail;
extern int dGeometry;
extern int dSize;
extern int dComplexity;
extern int dBlur;
extern int readyToDraw;

void setDefaults(int which);
void readRegistry();
void initControls(HWND hdlg);
// flux is the only saver of the thirteen whose window procedure returns LRESULT
// rather than LONG. Declaring it LONG here compiles and then fails to link.
LRESULT screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

namespace {

// DEFAULTS1 is "Regular", the preset the Linux command-line path starts from
// (flux.cpp:617) and the closest thing flux has to a plain default.
class Flux : public savertest::SaverFixture {
protected:
    void SetUp() override {
        rsRandGen().seed(savertest::kTestSeed);
        setDefaults(DEFAULTS1);
    }
};

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(FluxHarness, SaverBodyWasActuallyCompiled) {
    // Guards the WIN32-define trap. MSVC predefines _WIN32, not WIN32, and the
    // whole saver sits inside #ifdef WIN32; without it this links against an
    // empty translation unit and every other test passes vacuously.
    setDefaults(DEFAULTS1);
    EXPECT_EQ(dFluxes, 1);
    EXPECT_EQ(dParticles, 20);
    EXPECT_EQ(dTrail, 40);
    EXPECT_EQ(dGeometry, 2);
    EXPECT_EQ(dSize, 15);
}

TEST(FluxHarness, EveryPresetLeavesTheSettingsUsable) {
    // The six presets are the only way flux sets its defaults; a preset that
    // left a count at zero would divide by zero in initSaver.
    constexpr std::array presets = {DEFAULTS1, DEFAULTS2, DEFAULTS3, DEFAULTS4, DEFAULTS5, DEFAULTS6};
    for (int preset : presets) {
        setDefaults(preset);
        EXPECT_GT(dFluxes, 0) << "preset " << preset;
        EXPECT_GT(dParticles, 0) << "preset " << preset;
        EXPECT_GT(dTrail, 0) << "preset " << preset << ": lumdiff divides by this";
        EXPECT_GT(dSize, 0) << "preset " << preset;
        EXPECT_GE(dGeometry, 0) << "preset " << preset;
        EXPECT_LE(dGeometry, 2) << "preset " << preset;
    }
}

// --- a frame ---------------------------------------------------------------

TEST_F(Flux, FrameLeavesTheMatrixStackBalanced) {
    start();
    draw();
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

TEST_F(Flux, FramePairsBeginAndEnd) {
    start();
    draw();
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

TEST_F(Flux, PrimitiveVertexCountsAreLegal) {
    start();
    draw();
    EXPECT_TRUE(savertest::VertexCountsLegal());
}

TEST_F(Flux, EnableStateIsTheSameEveryFrame) {
    // flux is the exception to NoEnableStateLeaked. It configures its blend
    // state at the top of every frame to suit dGeometry (flux.cpp:445-462) and
    // never disables it again, where the other savers bracket their enables.
    // That is legal - glEnable is idempotent and each frame re-establishes what
    // it needs - so the invariant worth holding here is that the set does not
    // grow. A frame that enabled something the previous one did not is the bug
    // this would catch.
    start();
    draw();
    const std::vector<std::pair<unsigned, int>> first = glstub::trace().enables;

    glstub::reset();
    draw();
    const std::vector<std::pair<unsigned, int>> second = glstub::trace().enables;

    EXPECT_FALSE(first.empty()) << "the mode setup should enable something";
    EXPECT_EQ(first, second);
}

TEST_F(Flux, ReadsBackNoInvalidEnums) {
    start();
    draw();
    EXPECT_TRUE(savertest::NoInvalidEnums());
}

// --- geometry modes --------------------------------------------------------
//
// dGeometry picks how a particle is drawn: 0 emits a GL_POINTS block each,
// 1 and 2 build a display list in initSaver and call it per particle
// (flux.cpp:263, 683, 726). Only mode 0 puts vertices in the frame itself.

TEST_F(Flux, PointsModeDrawsOnePointPerParticle) {
    stop();
    dGeometry = 0;
    start();
    draw();

    EXPECT_GT(countPrimitives(GL_POINTS), 0);
    EXPECT_EQ(glstub::trace().totalVertices(),
              static_cast<unsigned long long>(countPrimitives(GL_POINTS)));
}

TEST_F(Flux, SphereModeDrawsThroughADisplayList) {
    stop();
    dGeometry = 1;
    start();
    draw();

    EXPECT_GT(glstub::trace().countCalls("glCallList"), 0);
    EXPECT_EQ(countPrimitives(GL_POINTS), 0);
}

TEST_F(Flux, LightModeDrawsThroughADisplayList) {
    stop();
    dGeometry = 2;
    start();
    draw();

    EXPECT_GT(glstub::trace().countCalls("glCallList"), 0);
}

// --- settings change what is drawn -----------------------------------------

TEST_F(Flux, MoreFluxesMeansMoreDrawing) {
    // Settings are changed only while nothing is allocated - see Task 16 in
    // docs/MAINTENANCE.md.
    stop();
    dGeometry = 0;
    dFluxes = 1;
    start();
    draw();
    const int few = countPrimitives(GL_POINTS);

    stop();
    dFluxes = 4;
    start();
    draw();
    const int many = countPrimitives(GL_POINTS);

    EXPECT_GT(few, 0);
    EXPECT_GT(many, few);
}

TEST_F(Flux, MoreParticlesMeansMoreDrawing) {
    stop();
    dGeometry = 0;
    dParticles = 5;
    start();
    draw();
    const int few = countPrimitives(GL_POINTS);

    stop();
    dParticles = 25;
    start();
    draw();
    const int many = countPrimitives(GL_POINTS);

    EXPECT_GT(few, 0);
    EXPECT_GT(many, few);
}

TEST_F(Flux, BlurDrawsTheFadingQuadInsteadOfClearing) {
    // With dBlur the frame is dimmed by drawing a translucent screen-filling
    // strip in an ortho projection rather than clearing (flux.cpp:402-423).
    stop();
    dBlur = 50;
    start();
    draw();

    EXPECT_GT(countPrimitives(GL_TRIANGLE_STRIP), 0);
    EXPECT_TRUE(savertest::MatrixStackBalanced())
        << "the blur path pushes on both the projection and modelview stacks";
}

// --- framework entry points ------------------------------------------------

TEST_F(Flux, IdleProcSkipsDrawingWhenNotReady) {
    start();
    readyToDraw = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().totalVertices(), 0u);
    readyToDraw = 1;
}

TEST(FluxFramework, ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy) {
    setDefaults(DEFAULTS1);
    readyToDraw = 0;

    screenSaverProc(testsupport::hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(readyToDraw, 1);

    screenSaverProc(testsupport::hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(readyToDraw, 0);
}

TEST(FluxFramework, ReadRegistryLeavesEveryValueUsable) {
    // Read-only: setDefaults runs first and the function returns early if the
    // key is absent, so this cannot disturb the machine. That early return also
    // means it covers little where the saver has never stored settings, CI
    // included - see the note in test_cyclone.cpp.
    setDefaults(DEFAULTS1);
    readRegistry();

    EXPECT_GT(dFluxes, 0);
    EXPECT_GT(dParticles, 0);
    EXPECT_GT(dTrail, 0);
    EXPECT_GT(dComplexity, 0) << "gluSphere is given dComplexity + 2 slices";
}

// --- dialog procedures -----------------------------------------------------

TEST(FluxDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_TRUE(savertest::AboutProcColoursTheWebPageLabel(aboutProc));
}

TEST(FluxDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(aboutProc));
}

TEST(FluxDialogs, InitControlsRunsWithoutADialog) {
    setDefaults(DEFAULTS1);
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(FluxDialogs, ConfigureDialogHandlesTheStandardMessages) {
    EXPECT_TRUE(savertest::ConfigureDialogInitialisesAndCancels(screenSaverConfigureDialog));
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(screenSaverConfigureDialog));
}

TEST(FluxDialogs, EveryPresetButtonRestoresItsPreset) {
    // flux has six preset buttons rather than one Defaults button, and each
    // arm of that switch is a separate line of coverage.
    constexpr std::array buttons = {DEFAULTS1, DEFAULTS2, DEFAULTS3, DEFAULTS4, DEFAULTS5, DEFAULTS6};
    for (int button : buttons) {
        setDefaults(button);
        const int expected = dFluxes;
        dFluxes = 99;

        screenSaverConfigureDialog(nullptr, WM_COMMAND, button, 0);

        EXPECT_EQ(dFluxes, expected) << "preset button " << button;
    }
}
