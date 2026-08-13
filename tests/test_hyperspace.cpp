/*
 * Tests for the hyperspace saver.
 *
 * hyperspace is compiled into this binary against the recording GL stub, so the
 * real draw path runs headless. Shared scaffolding - the fixture and the frame
 * invariants - lives in support/saver_test_common.h; what is here is what is
 * specific to hyperspace.
 *
 * It is the largest saver in the tree at ten translation units, and the one
 * that most needs the stub's matrix stack: its lens flares read the modelview
 * and projection back with glGetDoublev and put them through gluProject
 * (flare.cpp:187) to decide where on screen to draw. A stub returning zeros
 * there produces a divide by zero rather than a position.
 */

#include "support/saver_test_common.h"


#include "resource.h"

// hyperspace.cpp has no header; its contract with the framework is by name. See
// the note on cpp:S5421 in test_fieldlines.cpp - these are declarations, not
// definitions.
extern int dSpeed;
extern int dStars;
extern int dStarSize;
extern int dResolution;
extern int dDepth;
extern int dFov;
extern int dUseTunnels;
extern int dUseGoo;
extern int dShaders;
extern int readyToDraw;

// How many frames the caustic and cube-map animations are built from
// (hyperspace.cpp:92). Not a setting - no registry entry, no dialog control.
extern int numAnimTexFrames;

// Owned by tests/support/saver_shim.cpp, not by the saver.
extern int doingPreview;

void setDefaults();
void readRegistry();
void initControls(HWND hdlg);
LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

namespace {

class Hyperspace : public savertest::SaverFixture {
protected:
    void SetUp() override {
        setDefaults();
        // The shipped 2000 stars make every case slower for no extra coverage;
        // the default itself is asserted below.
        dStars = 100;

        // hyperspace builds its caustic textures and cube maps on the first
        // frame by rendering and reading them back, and at full size that is
        // twenty 256x256 frames over a 100x100 grid - about nineteen seconds
        // per case, which is not a CI cost worth paying for identical
        // coverage. doingPreview is the saver's own switch for exactly this
        // (hyperspace.cpp:115, 124) and drops it to 32x32, so it also walks
        // the preview branch in initSaver that nothing else reaches.
        doingPreview = 1;

        // Even at preview size the caustic build dominates: hyperspace was 35%
        // of the whole instrumented coverage run, at roughly 53 seconds a case,
        // almost all of it here. It is gated by dUseTunnels
        // (hyperspace.cpp:110), and only the two cases below are about tunnels
        // at all, so the rest opt out rather than each paying for a texture set
        // they never sample.
        //
        // Turning it on is opting in to that cost - see
        // BuildsItsCausticTextures and TunnelsAndGooCanBeTurnedOff, which are
        // what keep causticTextures.cpp and tunnel.cpp covered.
        dUseTunnels = 0;

        // The three cases that do want tunnels still pay for the caustic build,
        // and it is one render-and-readback per animation frame. Sixteen is the
        // floor, not a round number: causticTextures clamps numFrames up to
        // numKeys * 2 and the saver passes 8 keys (causticTextures.cpp:48), so
        // anything lower is silently ignored.
        numAnimTexFrames = 16;
    }

    void TearDown() override {
        savertest::SaverFixture::TearDown();
        doingPreview = 0;
        numAnimTexFrames = 20;
    }
};

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(HyperspaceHarness, SaverBodyWasActuallyCompiled) {
    // Guards the WIN32-define trap. MSVC predefines _WIN32, not WIN32, and the
    // whole saver sits inside #ifdef WIN32; without it this links against an
    // empty translation unit and every other test passes vacuously.
    setDefaults();
    EXPECT_EQ(dSpeed, 10);
    EXPECT_EQ(dStars, 2000);
    EXPECT_EQ(dStarSize, 10);
    EXPECT_EQ(dResolution, 10);
    EXPECT_EQ(dFov, 50);
}

TEST(HyperspaceHarness, KeepsShadersWhenTheExtensionsAreThere) {
    // initSaver turns dShaders off if initExtensions reports failure
    // (hyperspace.cpp:517-518). The stub advertises the three ARB extensions
    // its loader asks for and resolves their entry points, so the shader path
    // is the one under test - which is also the one that runs on any GPU made
    // since about 2002.
    //
    // The fallback is deliberately not exercised, because it does not work:
    // draw() calls glActiveTextureARB unconditionally at hyperspace.cpp:231-235
    // while every other use is inside if(dShaders), so with the pointers left
    // null it calls through address zero on the first frame. Recorded in
    // docs/MAINTENANCE.md.
    setDefaults();
    dStars = 100;
    ASSERT_EQ(dShaders, 1) << "setDefaults asks for shaders";

    initSaver(testsupport::hostWindow());
    EXPECT_EQ(dShaders, 1) << "and initExtensions must find them";
    cleanUp(testsupport::hostWindow());
}

TEST(HyperspaceHarness, CompilesItsShadersOnce) {
    setDefaults();
    dStars = 100;
    glstub::reset();

    initSaver(testsupport::hostWindow());

    EXPECT_GT(glstub::trace().countCalls("glCreateShaderObjectARB"), 0);
    EXPECT_GT(glstub::trace().countCalls("glLinkProgramARB"), 0);
    EXPECT_EQ(glstub::trace().countCalls("glCreateShaderObjectARB"),
              glstub::trace().countCalls("glCompileShaderARB"))
        << "every shader object created must also be compiled";

    cleanUp(testsupport::hostWindow());
}

// --- a frame ---------------------------------------------------------------

TEST_F(Hyperspace, FrameLeavesTheMatrixStackBalanced) {
    start();
    draw();
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

TEST_F(Hyperspace, FramePairsBeginAndEnd) {
    start();
    draw();
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

TEST_F(Hyperspace, PrimitiveVertexCountsAreLegal) {
    start();
    draw();
    EXPECT_TRUE(savertest::VertexCountsLegal());
}

TEST_F(Hyperspace, EnableStateIsTheSameEveryFrame) {
    // Like flux, hyperspace sets the state it wants at the top of a frame and
    // leaves it: blending goes on at hyperspace.cpp:229 and is never switched
    // off. glEnable is idempotent, so the invariant that matters is that the
    // set does not grow - a frame enabling something the previous one did not
    // is the bug this catches.
    start();
    draw();
    const std::vector<std::pair<unsigned, int>> first = glstub::trace().enables;

    glstub::reset();
    draw();
    const std::vector<std::pair<unsigned, int>> second = glstub::trace().enables;

    EXPECT_FALSE(first.empty());
    EXPECT_EQ(first, second);
}

TEST_F(Hyperspace, ReadsBackNoInvalidEnums) {
    // hyperspace makes more matrix readbacks than any other saver - billboard
    // orientation, the modelview and projection for gluProject, and the
    // viewport for the flares.
    start();
    draw();
    EXPECT_TRUE(savertest::NoInvalidEnums());
}

TEST_F(Hyperspace, DrawsTheStarfieldAsTriangleStrips) {
    // Every star is a stretched particle drawn as one or two strips
    // (stretchedParticle.cpp:105, 117).
    start();
    draw();

    EXPECT_GT(countPrimitives(GL_TRIANGLE_STRIP), 0);
    EXPECT_GT(glstub::trace().totalVertices(), 0u);
}

// --- the matrix stack ------------------------------------------------------

TEST_F(Hyperspace, ReadsBackAProjectionItCanProjectWith) {
    // The flares call gluProject with whatever glGetDoublev returned, so the
    // projection has to be a real perspective matrix rather than zeros.
    start();
    draw();

    float projection[16];
    glstub::currentMatrix(GL_PROJECTION, projection);

    EXPECT_FLOAT_EQ(projection[11], -1.0f) << "the fourth row must take w from z";
    EXPECT_GT(projection[0], 0.0f);
    EXPECT_GT(projection[5], 0.0f);
    for (int i = 0; i < 16; ++i) {
        EXPECT_TRUE(std::isfinite(projection[i])) << "projection element " << i;
    }
}

TEST_F(Hyperspace, WiderFieldOfViewFlattensTheProjection) {
    stop();
    dFov = 30;
    start();
    float narrow[16];
    glstub::currentMatrix(GL_PROJECTION, narrow);

    stop();
    dFov = 90;
    start();
    float wide[16];
    glstub::currentMatrix(GL_PROJECTION, wide);

    EXPECT_GT(narrow[0], wide[0]);
}

TEST_F(Hyperspace, LeavesTheModelviewFiniteAfterAFrame) {
    start();
    draw();

    float modelview[16];
    glstub::currentMatrix(GL_MODELVIEW, modelview);
    for (int i = 0; i < 16; ++i) {
        EXPECT_TRUE(std::isfinite(modelview[i])) << "modelview element " << i;
    }
}

// --- settings change what is drawn -----------------------------------------

TEST_F(Hyperspace, MoreStarsMeansMoreDrawing) {
    // Settings are changed only while nothing is allocated - see Task 16 in
    // docs/MAINTENANCE.md.
    stop();
    dStars = 50;
    start();
    draw();
    const int few = countPrimitives(GL_TRIANGLE_STRIP);

    stop();
    dStars = 300;
    start();
    draw();
    const int many = countPrimitives(GL_TRIANGLE_STRIP);

    EXPECT_GT(few, 0);
    EXPECT_GT(many, few);
}

TEST_F(Hyperspace, TunnelsAndGooCanBeTurnedOff) {
    stop();
    dUseTunnels = 0;
    dUseGoo = 0;
    start();
    draw();
    const int without = countPrimitives(GL_TRIANGLE_STRIP);

    stop();
    dUseTunnels = 1;
    dUseGoo = 1;
    start();
    draw();
    const int with = countPrimitives(GL_TRIANGLE_STRIP);

    EXPECT_GT(without, 0) << "the stars are drawn either way";
    EXPECT_NE(with, without);
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

TEST_F(Hyperspace, RestartingRebuildsTheGeneratedTextures) {
    // Regression guard. The caustic textures and cube maps are built on the
    // first frame and deleted by cleanUp, so the flag that says "already
    // built" has to be cleared there too. It used to be a static inside
    // draw(), which left the second cycle drawing through freed pointers -
    // reliably an access violation.
    //
    // dUseTunnels has to be on: without it there are no caustic textures to
    // free and rebuild, and the guard would pass while testing nothing.
    stop();
    dUseTunnels = 1;
    start();
    draw();
    stop();

    start();
    EXPECT_NO_FATAL_FAILURE(draw());
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

TEST_F(Hyperspace, BuildsItsCausticTexturesOnTheFirstFrame) {
    // The caustic animation is rendered into a texture set and read back, and
    // this is the only test that reaches causticTextures.cpp at all.
    //
    // It happens on the first draw rather than in initSaver, because the
    // textures are drawn and read back from the framebuffer and so need the
    // context to be live (hyperspace.cpp:110-121). An earlier version of this
    // test used startCapturingSetup() and asserted on texture calls, which
    // initFlares satisfies on its own - so it passed without ever building a
    // caustic texture.
    // startCapturingSetup rather than start, because start() draws the warm-up
    // frame and then clears the trace - which is exactly the frame the build
    // happens in.
    stop();
    dUseTunnels = 1;
    startCapturingSetup();
    draw();

    // One render-and-readback per animation frame (causticTextures.cpp:198),
    // and nothing else in hyperspace reads the framebuffer back - so the count
    // is exactly numAnimTexFrames.
    //
    // Not asserted against gluBuild2DMipmaps: the wavy normal cube maps upload
    // through it too, six faces per frame, so that count is far higher.
    EXPECT_EQ(glstub::trace().countCalls("glReadPixels"), numAnimTexFrames);
    EXPECT_GE(glstub::trace().countCalls("gluBuild2DMipmaps"), numAnimTexFrames);
    EXPECT_GT(glstub::trace().texturesGenerated, 0);
}

// --- framework entry points ------------------------------------------------

TEST_F(Hyperspace, IdleProcSkipsDrawingWhenNotReady) {
    start();
    readyToDraw = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().totalVertices(), 0u);
    readyToDraw = 1;
}

TEST(HyperspaceFramework, ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy) {
    setDefaults();
    dStars = 100;
    readyToDraw = 0;

    screenSaverProc(testsupport::hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(readyToDraw, 1);

    screenSaverProc(testsupport::hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(readyToDraw, 0);
}

TEST(HyperspaceFramework, ReadRegistryLeavesEveryValueUsable) {
    // Read-only: setDefaults runs first and the function returns early if the
    // key is absent, so this cannot disturb the machine. That early return also
    // means it covers little where the saver has never stored settings, CI
    // included - see the note in test_cyclone.cpp.
    setDefaults();
    readRegistry();

    EXPECT_GT(dStars, 0) << "the star array is allocated with this count";
    EXPECT_GT(dResolution, 0) << "the goo mesh is sized from this";
    EXPECT_GT(dDepth, 0);
    EXPECT_GT(dFov, 0) << "the projection divides by half its tangent";
}

// --- dialog procedures -----------------------------------------------------

TEST(HyperspaceDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_TRUE(savertest::AboutProcColoursTheWebPageLabel(aboutProc));
}

TEST(HyperspaceDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(aboutProc));
}

TEST(HyperspaceDialogs, InitControlsRunsWithoutADialog) {
    setDefaults();
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(HyperspaceDialogs, ConfigureDialogHandlesTheStandardMessages) {
    EXPECT_TRUE(savertest::ConfigureDialogInitialisesAndCancels(screenSaverConfigureDialog));
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(screenSaverConfigureDialog));
}

TEST(HyperspaceDialogs, ConfigureDialogRestoresDefaults) {
    setDefaults();
    const int defaultStars = dStars;
    dStars = 99;

    screenSaverConfigureDialog(nullptr, WM_COMMAND, DEFAULTS, 0);

    EXPECT_EQ(dStars, defaultStars);
}
