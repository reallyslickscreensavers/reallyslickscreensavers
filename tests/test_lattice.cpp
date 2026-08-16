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
 * (lattice.cpp:693-699) and steers with glLoadMatrixf, so a stub that only
 * recorded those calls could not say anything about where the camera ended up.
 */

#include "support/saver_test_common.h"

#include <array>
#include <cstring>
#include <new>

#include "camera.h"
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
        // frustum-culls each one (lattice.cpp:590-593), so the shipped 5 tests
        // 3,375 cells a frame against 729 at 2. The cells that survive culling
        // are drawn from the same display lists either way, so the same code
        // runs. The default is asserted in the harness test, and the case that
        // compares depths sets its own values.
        dDepth = 2;
    }
};

// Triangle strips carrying exactly n vertices. Counting strips alone cannot
// answer this: initSaver constructs rsText (lattice.cpp:806) and
// rsText::rsText() compiles 128 four-vertex GL_TRIANGLE_STRIP glyph lists
// (libs/rsText/rsText.cpp:44), so the trace is never empty and never free of
// strips, whatever makeTorus does.
int stripsWithVertices(unsigned n) {
    int count = 0;
    for (const auto& p : glstub::trace().primitives) {
        if (p.mode == GL_TRIANGLE_STRIP && p.vertices == n) ++count;
    }
    return count;
}

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

// --- the camera constructor -------------------------------------------------

TEST(LatticeCamera, ConstructorLeavesNoIndeterminateFields) {
    // camera has no virtual functions, so placement new over a raw buffer is
    // well-defined here. The memset is the point: it makes the assertion hold
    // in Release as well as Debug instead of leaning on /RTC1's 0xCC fill.
    alignas(camera) unsigned char storage[sizeof(camera)];
    std::memset(storage, 0x5A, sizeof storage);
    camera* c = new (storage) camera;

    EXPECT_FLOAT_EQ(c->farplane, 0.0f);
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 3; ++j) {
            EXPECT_FLOAT_EQ(c->cullVec[i][j], 0.0f) << "cullVec[" << i << "][" << j << "]";
        }
    }

    c->~camera();
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

TEST_F(Lattice, DisablesTextureCoordinateGenerationOnlyWhenItEnabledIt) {
    // Was a pinned defect: the enable was conditional on a reflective texture
    // and the disable was not, so a frame with any other texture disabled
    // something it never enabled and the net came out at -1. Both sides now
    // hang off one predicate - Task 22 in docs/MAINTENANCE.md.
    //
    // Textures 2 to 6 - crystal, chrome, brass, shiny, ghostly - are the
    // reflective ones and the only ones that sphere-map. dTexture 9 means
    // "random" and initSaver resolves it to rsRandi(9) (lattice.cpp:679-680),
    // so 0 to 8 is every value a frame can ever see.
    //
    // countEnables is asserted alongside netEnable because a net of 0 is also
    // what an unconditional enable paired with an unconditional disable would
    // give; the pair has to be absent, not merely balanced.
    //
    // dDensity is dropped for the same reason the fixture drops dDepth: this
    // restarts the saver nine times and the display lists are the expensive
    // part. It changes no enable state.
    for (int texture = 0; texture <= 8; ++texture) {
        stop();
        dTexture = texture;
        dDensity = 20;
        start();
        draw();

        const int expected = (texture >= 2 && texture <= 6) ? 1 : 0;
        EXPECT_EQ(glstub::trace().countEnables(GL_TEXTURE_GEN_S), expected) << "dTexture " << texture;
        EXPECT_EQ(glstub::trace().countEnables(GL_TEXTURE_GEN_T), expected) << "dTexture " << texture;
        EXPECT_EQ(glstub::trace().netEnable(GL_TEXTURE_GEN_S), 0) << "dTexture " << texture;
        EXPECT_EQ(glstub::trace().netEnable(GL_TEXTURE_GEN_T), 0) << "dTexture " << texture;
    }
}

TEST_F(Lattice, LeaksNoOtherEnableState) {
    // Before Task 22 the texture-generation pair was an exception to this and
    // the case above held it separately. It is not any more, so the whole
    // frame balances with a plain texture and with a reflective one alike.
    //
    // Still a per-suite assertion rather than a repo-wide rule: flux enables
    // its blend state at the top of every frame and never disables it, which
    // is legal and is held by Flux.EnableStateIsTheSameEveryFrame instead.
    constexpr std::array textures = {0, 3};  // plain, then reflective
    for (int texture : textures) {
        stop();
        dTexture = texture;
        start();
        draw();
        EXPECT_TRUE(savertest::NoEnableStateLeaked()) << "dTexture " << texture;
    }
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
    // (lattice.cpp:607). The frame itself emits no vertices.
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

TEST_F(Lattice, BuildsNoDegenerateTorusWhenLongitudeIsZero) {
    // makeTorus is only ever called from makeLatticeObjects at setup
    // (lattice.cpp:311-369), so startCapturingSetup() - which resets the
    // trace, then runs initSaver, and draws no warm-up frame - is the only
    // place those primitives exist.
    //
    // Control: a two-segment torus. 2*longitude + 2 = 6 vertices per strip,
    // which is legal and, importantly, is not 4 - so these cannot be confused
    // with rsText's glyph strips.
    stop();
    dLongitude = 2;
    dDensity = 20;
    startCapturingSetup();
    const int sixes = stripsWithVertices(6);
    const int withTorus = countPrimitives(GL_TRIANGLE_STRIP);
    EXPECT_GT(sixes, 0) << "makeTorus did not run at all";

    stop();
    dLongitude = 0;
    dDensity = 20;
    startCapturingSetup();
    EXPECT_EQ(stripsWithVertices(2), 0) << "a tri-strip closed from unwritten old* values";
    EXPECT_LT(countPrimitives(GL_TRIANGLE_STRIP), withTorus) << "the guard did not fire";
    EXPECT_GT(countPrimitives(GL_TRIANGLE_STRIP), 0) << "setup did not run at all";
    EXPECT_GT(glstub::trace().countCalls("glNewList"), 0) << "the display lists were still built";
    EXPECT_TRUE(savertest::VertexCountsLegal());
}

TEST_F(Lattice, FogIsSetUpOnlyWhenAskedFor) {
    // Fog is configured once in initSaver (lattice.cpp:729-735), so it has to
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
