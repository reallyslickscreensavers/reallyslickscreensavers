/*
 * Tests for the lattice saver.
 *
 * lattice.cpp is compiled into this binary against the recording GL stub, so
 * the real draw path runs headless. Shared scaffolding - the fixture and the
 * frame invariants - lives in support/saver_test_common.h; what is here is what
 * is specific to lattice.
 *
 * This is the first suite to lean on the stub's matrix stack. lattice loads its
 * projection as a hand-built matrix rather than through gluPerspective
 * (lattice.cpp:690-696) and steers with glLoadMatrixf, so a stub that only
 * recorded those calls could not say anything about where the camera ended up.
 */

#include "support/saver_test_common.h"

#include <array>

#include "resource.h"

// lattice.cpp has no header; its contract with the framework is by name. See
// the note on cpp:S5421 in test_fieldlines.cpp - these are declarations, not
// definitions.
extern int dLongitude;
extern int dLatitude;
extern int dThick;
extern int dDensity;
extern int dDepth;
extern int dFov;
extern int dPathrand;
extern int dTexture;
extern BOOL dSmooth;
extern BOOL dFog;
extern int readyToDraw;

void setDefaults(int which);
void readRegistry();
void initControls(HWND hdlg);
LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

namespace {

// DEFAULTS1 is "Regular", lattice's baseline preset.
class Lattice : public savertest::SaverFixture {
protected:
    void SetUp() override {
        setDefaults(DEFAULTS1);

        // draw() walks a cube of cells (2 * (dDepth + 2) + 1) on a side and
        // frustum-culls each one (lattice.cpp:589-592), so the shipped 5 tests
        // 3,375 cells a frame against 729 at 2. The cells that survive culling
        // are drawn from the same display lists either way, so the same code
        // runs. The default is asserted in the harness test, and the case that
        // compares depths sets its own values.
        dDepth = 2;
    }
};

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(LatticeHarness, SaverBodyWasActuallyCompiled) {
    // Guards the WIN32-define trap. MSVC predefines _WIN32, not WIN32, and the
    // whole saver sits inside #ifdef WIN32; without it this links against an
    // empty translation unit and every other test passes vacuously.
    setDefaults(DEFAULTS1);
    EXPECT_EQ(dLongitude, 16);
    EXPECT_EQ(dLatitude, 8);
    EXPECT_EQ(dThick, 50);
    EXPECT_EQ(dDepth, 5);
    EXPECT_EQ(dFov, 90);
}

TEST(LatticeHarness, EveryPresetLeavesTheSettingsUsable) {
    constexpr std::array presets = {DEFAULTS1, DEFAULTS2, DEFAULTS3, DEFAULTS4, DEFAULTS5, DEFAULTS6};
    for (int preset : presets) {
        setDefaults(preset);
        EXPECT_GT(dLongitude, 0) << "preset " << preset << ": the torus is built from this";
        EXPECT_GT(dLatitude, 0) << "preset " << preset;
        EXPECT_GT(dDepth, 0) << "preset " << preset << ": the projection divides by it";
        EXPECT_GT(dFov, 0) << "preset " << preset << ": and by half its tangent";
        EXPECT_LT(dPathrand, 11) << "preset " << preset << ": rsRandi(11 - dPathrand) needs headroom";
    }
}

// --- a frame ---------------------------------------------------------------

TEST_F(Lattice, FrameLeavesTheMatrixStackBalanced) {
    start();
    draw();
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

TEST_F(Lattice, FramePairsBeginAndEnd) {
    start();
    draw();
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

TEST_F(Lattice, PrimitiveVertexCountsAreLegal) {
    start();
    draw();
    EXPECT_TRUE(savertest::VertexCountsLegal());
}

TEST_F(Lattice, DisablesTextureCoordinateGenerationItNeverEnabled) {
    // DEFECT, pinned rather than fixed - lattice is the exception to
    // NoEnableStateLeaked, and in the harmless direction.
    //
    // The enable is conditional on dTexture being one of the reflective ones
    // (lattice.cpp:580-585) while the matching disable is unconditional
    // (lattice.cpp:613-614). With any other texture the frame disables
    // something it never enabled, so the net comes out at -1.
    //
    // Disabling an already-disabled capability is a no-op in GL, which is why
    // this has never shown. Fixing it means moving the disable inside the same
    // condition. Recorded in docs/MAINTENANCE.md.
    stop();
    dTexture = 0;
    start();
    draw();

    EXPECT_EQ(glstub::trace().netEnable(GL_TEXTURE_GEN_S), -1)
        << "if this now fails the disable has been made conditional too";
    EXPECT_EQ(glstub::trace().netEnable(GL_TEXTURE_GEN_T), -1);
}

TEST_F(Lattice, LeaksNoOtherEnableState) {
    // Everything except the texture-generation pair above must balance.
    stop();
    dTexture = 3;  // reflective, so the enable and disable pair up
    start();
    draw();

    EXPECT_TRUE(savertest::NoEnableStateLeaked());
}

TEST_F(Lattice, ReadsBackNoInvalidEnums) {
    // Regression guard for the culling block removed from draw(): it queried
    // GL_MODELVIEW, the matrix mode, where GL_MODELVIEW_MATRIX was meant, and
    // fed two locals nothing ever read.
    start();
    draw();
    EXPECT_TRUE(savertest::NoInvalidEnums());
}

TEST_F(Lattice, DrawsTheLatticeThroughDisplayLists) {
    // Every cell of the lattice is one of the shapes compiled into display
    // lists in initSaver (lattice.cpp:302), called per visible cell
    // (lattice.cpp:606). The frame itself emits no vertices.
    start();
    draw();

    EXPECT_GT(glstub::trace().countCalls("glCallList"), 0);
    EXPECT_EQ(glstub::trace().totalVertices(), 0u);
}

// --- the matrix stack ------------------------------------------------------

TEST_F(Lattice, BuildsAProjectionThatCanBeReadBack) {
    // initSaver loads its projection with glLoadMatrixf rather than
    // gluPerspective, so this is the stub's matrix stack answering with what
    // the saver actually built.
    start();

    std::array<float, 16> projection{};
    glstub::currentMatrix(GL_PROJECTION, projection.data());

    // A perspective projection: w comes from -z, and the near/far terms are
    // negative. cot(fov/2) with the default 90 degree field of view is 1.
    EXPECT_FLOAT_EQ(projection[11], -1.0f) << "the fourth row must take w from z";
    EXPECT_LT(projection[10], 0.0f);
    EXPECT_LT(projection[14], 0.0f);
    EXPECT_NEAR(projection[0], 1.0f, 1e-5f) << "cot(45 degrees)";
    EXPECT_GT(projection[5], 0.0f);
}

TEST_F(Lattice, WiderFieldOfViewFlattensTheProjection) {
    // cot(fov/2) shrinks as the field of view widens, which is the only thing
    // dFov changes.
    stop();
    dFov = 60;
    start();
    std::array<float, 16> narrow{};
    glstub::currentMatrix(GL_PROJECTION, narrow.data());

    stop();
    dFov = 120;
    start();
    std::array<float, 16> wide{};
    glstub::currentMatrix(GL_PROJECTION, wide.data());

    EXPECT_GT(narrow[0], wide[0]);
}

TEST_F(Lattice, LeavesTheCameraSomewhereFiniteAfterAFrame) {
    start();
    draw();

    std::array<float, 16> modelview{};
    glstub::currentMatrix(GL_MODELVIEW, modelview.data());
    for (int i = 0; i < 16; ++i) {
        EXPECT_TRUE(std::isfinite(modelview[i])) << "modelview element " << i;
    }
}

// --- settings change what is drawn -----------------------------------------

TEST_F(Lattice, DeeperViewMeansMoreCellsDrawn) {
    // Settings are changed only while nothing is allocated - see Task 16 in
    // docs/MAINTENANCE.md.
    stop();
    dDepth = 3;
    start();
    draw();
    const int shallow = glstub::trace().countCalls("glCallList");

    stop();
    dDepth = 6;
    start();
    draw();
    const int deep = glstub::trace().countCalls("glCallList");

    EXPECT_GT(shallow, 0);
    EXPECT_GT(deep, shallow);
}

TEST_F(Lattice, DenserLatticeMeansMoreGeometryInTheDisplayLists) {
    // dDensity decides how many struts each cell gets (lattice.cpp:305-361),
    // not how many cells are drawn - so it changes what goes into the display
    // lists at setup, and the frame itself looks identical.
    stop();
    dDensity = 20;
    startCapturingSetup();
    const unsigned long long sparse = glstub::trace().totalVertices();

    stop();
    dDensity = 95;
    startCapturingSetup();
    const unsigned long long dense = glstub::trace().totalVertices();

    EXPECT_GT(sparse, 0u);
    EXPECT_GT(dense, sparse);
}

TEST_F(Lattice, BuildsTheTorusFromLongitudeAndLatitude) {
    stop();
    dLongitude = 8;
    dLatitude = 4;
    startCapturingSetup();
    const unsigned long long coarse = glstub::trace().totalVertices();

    stop();
    dLongitude = 24;
    dLatitude = 12;
    startCapturingSetup();
    const unsigned long long fine = glstub::trace().totalVertices();

    EXPECT_GT(coarse, 0u);
    EXPECT_GT(fine, coarse);
}

TEST_F(Lattice, EnvironmentMappedTexturesTurnOnCoordinateGeneration) {
    // Textures 2 to 6 are the reflective ones and switch on sphere mapping
    // around the lattice (lattice.cpp:583-584, 613-614).
    stop();
    dTexture = 0;
    start();
    draw();
    EXPECT_EQ(glstub::trace().countEnables(GL_TEXTURE_GEN_S), 0);

    stop();
    dTexture = 3;
    start();
    draw();
    EXPECT_EQ(glstub::trace().countEnables(GL_TEXTURE_GEN_S), 1);
    EXPECT_EQ(glstub::trace().countEnables(GL_TEXTURE_GEN_T), 1);
    EXPECT_TRUE(savertest::NoEnableStateLeaked());
}

TEST_F(Lattice, FogIsSetUpOnlyWhenAskedFor) {
    // Fog is configured once in initSaver (lattice.cpp:726-732), so it has to
    // be captured there rather than in a frame.
    stop();
    dFog = FALSE;
    startCapturingSetup();
    EXPECT_EQ(glstub::trace().countCalls("glFogf"), 0);
    EXPECT_EQ(glstub::trace().countEnables(GL_FOG), 0);

    stop();
    dFog = TRUE;
    startCapturingSetup();
    EXPECT_EQ(glstub::trace().countCalls("glFogf"), 3) << "mode, start and end";
    EXPECT_EQ(glstub::trace().countCalls("glFogfv"), 1) << "colour";
    EXPECT_EQ(glstub::trace().countEnables(GL_FOG), 1);
}

// --- framework entry points ------------------------------------------------

TEST_F(Lattice, IdleProcSkipsDrawingWhenNotReady) {
    start();
    readyToDraw = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().countCalls("glCallList"), 0);
    readyToDraw = 1;
}

TEST(LatticeFramework, ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy) {
    setDefaults(DEFAULTS1);
    readyToDraw = 0;

    screenSaverProc(testsupport::hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(readyToDraw, 1);

    screenSaverProc(testsupport::hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(readyToDraw, 0);
}

TEST(LatticeFramework, ReadRegistryLeavesEveryValueUsable) {
    // Read-only: setDefaults runs first and the function returns early if the
    // key is absent, so this cannot disturb the machine. That early return also
    // means it covers little where the saver has never stored settings, CI
    // included - see the note in test_cyclone.cpp.
    setDefaults(DEFAULTS1);
    readRegistry();

    EXPECT_GT(dLongitude, 0);
    EXPECT_GT(dLatitude, 0);
    EXPECT_GT(dDepth, 0);
    EXPECT_GT(dFov, 0);
    EXPECT_LT(dPathrand, 11)
        << "draw() calls rsRandi(11 - dPathrand); rslibs L4 made that safe, but "
           "the setting still has to stay in range";
}

// --- dialog procedures -----------------------------------------------------

TEST(LatticeDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_TRUE(savertest::AboutProcColoursTheWebPageLabel(aboutProc));
}

TEST(LatticeDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(aboutProc));
}

TEST(LatticeDialogs, InitControlsRunsWithoutADialog) {
    setDefaults(DEFAULTS1);
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(LatticeDialogs, ConfigureDialogHandlesTheStandardMessages) {
    EXPECT_TRUE(savertest::ConfigureDialogInitialisesAndCancels(screenSaverConfigureDialog));
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(screenSaverConfigureDialog));
}

TEST(LatticeDialogs, EveryPresetButtonRestoresItsPreset) {
    constexpr std::array buttons = {DEFAULTS1, DEFAULTS2, DEFAULTS3, DEFAULTS4, DEFAULTS5, DEFAULTS6};
    for (int button : buttons) {
        setDefaults(button);
        const int expected = dLongitude;
        dLongitude = 99;

        screenSaverConfigureDialog(nullptr, WM_COMMAND, button, 0);

        EXPECT_EQ(dLongitude, expected) << "preset button " << button;
    }
}
