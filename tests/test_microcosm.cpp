/*
 * Tests for the microcosm saver.
 *
 * microcosm.cpp is compiled into this binary against the recording GL stub, so
 * the real draw path runs headless. Shared scaffolding - the fixture and the
 * frame invariants - lives in support/saver_test_common.h; what is here is what
 * is specific to microcosm.
 *
 * It was the saver expected to be hardest, because it computes its implicit
 * surfaces on two worker threads driven by four while(1) loops
 * (microcosm.cpp:275-342), and a test reaching the thread-start path would
 * hang. It turned out not to need any of that: gUseThreads is an ordinary
 * global, and the single-threaded branch it selects is a complete
 * implementation rather than a fallback stub.
 */

#include "support/saver_test_common.h"


#include <array>
#include <type_traits>
#include <vector>

// For the leak check in GizmoDestructionIsVirtual. Debug-only by construction:
// the whole header is a no-op without _DEBUG.
#include <crtdbg.h>

#include "resource.h"
// For the gizmo list below. It is the saver's own header, reached through the
// module include directory the same way resource.h is.
#include "gizmo.h"
// One gizmo per ownership shape, for GizmoDestructionIsVirtual below. Every
// other gizmo is one of these four in a different size.
#include "brain.h"        // owns three arrays of pointers into mShapes
#include "cube.h"         // owns nothing beyond mShapes
#include "orbit.h"        // three named aliases into mShapes
#include "ringOfTori.h"   // one array of pointers into mShapes
#include "microcosmSettings.h"

// microcosm.cpp has no header; its contract with the framework is by name. See
// the note on cpp:S5421 in test_fieldlines.cpp - these are declarations, not
// definitions.
extern int dSingleTime;
extern int dKaleidoscopeTime;
extern int dBackground;
extern int dResolution;
extern int dDepth;
extern int dFov;
extern int dGizmoSpeed;
extern int dColorSpeed;
extern int dCameraSpeed;
extern int dShaders;
extern int dFog;
extern int gMode;
extern float gModeTransition;
extern int readyToDraw;

// Chooses between the threaded and single-threaded compute paths
// (microcosm.cpp:163). Not a setting - it is never read from the registry and
// has no dialog control.
extern bool gUseThreads;

// Owned by tests/support/saver_shim.cpp, not by the saver.
extern int doingPreview;

// Built by initSaver and indexed by gGizmoIndex.
extern std::vector<Gizmo*> gizmos;
extern unsigned int gGizmoIndex;

// Gates the Tennis easter egg, which chooseGizmo hides by dropping the last
// entry off its random range until this turns true (microcosm.cpp:371).
extern bool gTennisAvailable;

void setDefaults(int which);
void readRegistry();
void initControls(HWND hdlg);

// The direct selector. chooseSpecificGizmo is not: it is the keypad handler for
// typing a two-digit gizmo number, so it ignores anything above 9 and needs two
// calls to select anything (microcosm.cpp:1456).
//
// The default argument lives on the definition, which is not part of the mangled
// name, so declaring it without one links fine.
void chooseGizmo(int index);
LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);
INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm);

namespace {

class Microcosm : public savertest::SaverFixture {
protected:
    void SetUp() override {
        setDefaults(0);

        // Must be set before initSaver: the thread-start path is at
        // microcosm.cpp:1108-1120 and there is no way back out of it.
        gUseThreads = false;

        // The saver's own switch for a cheap configuration, which initSaver
        // applies by clamping dResolution to 20 and dDepth to 2
        // (microcosm.cpp:938-943). A 50-cubed marching-cubes volume per frame
        // on one thread is otherwise seconds per case for identical coverage.
        doingPreview = 1;

        // draw() picks the surface function from gModeTransition, and below 1.0
        // that is a blend between single and kaleidoscope mode
        // (microcosm.cpp:538-552). It only climbs with elapsed time, and frames
        // drawn back to back in a test see a frameTime of microseconds, so
        // without this it stays at 0 and the volume never yields an isosurface.
        // 1.0 is the steady state the saver spends nearly all its time in.
        gModeTransition = 1.0f;
    }

    void TearDown() override {
        savertest::SaverFixture::TearDown();
        doingPreview = 0;
        gUseThreads = true;
        gModeTransition = 0.0f;
    }
};

// The settings this saver reads and the ranges its own header declares.
// Built once: both cases below assert against the same list, and writing it
// out twice is what tripped the duplication gate.
std::vector<savertest::RangedSetting> declaredRanges() {
    return {
        savertest::Ranged("dKaleidoscopeTime", dKaleidoscopeTime, microcosmSettings::kKaleidoscopeTime),
        savertest::Ranged("dSingleTime", dSingleTime, microcosmSettings::kSingleTime),
        savertest::Ranged("dBackground", dBackground, microcosmSettings::kBackground),
        savertest::Ranged("dResolution", dResolution, microcosmSettings::kResolution),
        savertest::Ranged("dDepth", dDepth, microcosmSettings::kDepth),
        savertest::Ranged("dFov", dFov, microcosmSettings::kFov),
        savertest::Ranged("dGizmoSpeed", dGizmoSpeed, microcosmSettings::kGizmoSpeed),
        savertest::Ranged("dColorSpeed", dColorSpeed, microcosmSettings::kColorSpeed),
        savertest::Ranged("dCameraSpeed", dCameraSpeed, microcosmSettings::kCameraSpeed),
        savertest::Ranged("dShaders", dShaders, microcosmSettings::kShaders),
        savertest::Ranged("dFog", dFog, microcosmSettings::kFog),
    };
}

}  // namespace

// --- the harness itself ----------------------------------------------------

TEST(MicrocosmHarness, SaverBodyWasActuallyCompiled) {
    // Guards the WIN32-define trap. MSVC predefines _WIN32, not WIN32, and the
    // whole saver sits inside #ifdef WIN32; without it this links against an
    // empty translation unit and every other test passes vacuously.
    setDefaults(0);
    EXPECT_EQ(dSingleTime, 60);
    EXPECT_EQ(dKaleidoscopeTime, 60);
    EXPECT_EQ(dResolution, 50);
    EXPECT_EQ(dDepth, 4);
    EXPECT_EQ(dFov, 60);
}

TEST(MicrocosmHarness, EveryPresetLeavesTheSettingsUsable) {
    for (int preset = 0; preset <= 2; ++preset) {
        setDefaults(preset);
        EXPECT_GT(dResolution, 0) << "preset " << preset << ": the volume is sized from this";
        EXPECT_GT(dDepth, 0) << "preset " << preset;
        EXPECT_GT(dFov, 0) << "preset " << preset << ": the projection divides by half its tangent";
        EXPECT_GT(dSingleTime + dKaleidoscopeTime, 0)
            << "preset " << preset << ": one of the two modes has to last a while";
        EXPECT_TRUE(savertest::SettingsWithinDeclaredRanges(declaredRanges())) << "preset " << preset;
    }
}

// --- the threading switch --------------------------------------------------

TEST_F(Microcosm, ComputesItsSurfacesWithoutWorkerThreads) {
    // The point of the whole suite: the single-threaded branch
    // (microcosm.cpp:592-605) calls makeSurface inline and produces a real
    // frame, so nothing here is a reduced version of what ships.
    //
    // If the branch is ever removed this test hangs rather than fails, which is
    // why the flag is checked as well - a saver that turned threading back on
    // would show up here first.
    start();
    draw();

    EXPECT_FALSE(gUseThreads) << "nothing in the saver may turn threading back on";
    EXPECT_GT(glstub::trace().countCalls("glDrawElements"), 0)
        << "the marching-cubes mesh is drawn through vertex arrays";
    EXPECT_GT(glstub::trace().totalArrayVertices(), 0u);
}

// --- a frame ---------------------------------------------------------------

TEST_F(Microcosm, FrameLeavesTheMatrixStackBalanced) {
    start();
    draw();
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

TEST_F(Microcosm, FramePairsBeginAndEnd) {
    start();
    draw();
    EXPECT_TRUE(savertest::PrimitivesPaired());
}

TEST_F(Microcosm, PrimitiveVertexCountsAreLegal) {
    start();
    draw();
    EXPECT_TRUE(savertest::VertexCountsLegal());
}

TEST_F(Microcosm, ReadsBackNoInvalidEnums) {
    start();
    draw();
    EXPECT_TRUE(savertest::NoInvalidEnums());
}

// --- shaders ---------------------------------------------------------------

TEST(MicrocosmFramework, KeepsShadersWhenTheExtensionsAreThere) {
    // initSaver turns dShaders off if initExtensions reports failure
    // (microcosm.cpp:927-928). The stub advertises GL_ARB_multitexture and
    // GL_ARB_shader_objects and resolves their entry points, so the shader path
    // is the one under test - the one that runs on any GPU made since about
    // 2002. microcosm has a real fallback for the other case
    // (microcosm.cpp:634), unlike hyperspace.
    setDefaults(0);
    gUseThreads = false;
    doingPreview = 1;
    ASSERT_EQ(dShaders, 1);

    initSaver(testsupport::hostWindow());
    EXPECT_EQ(dShaders, 1);
    cleanUp(testsupport::hostWindow());

    doingPreview = 0;
    gUseThreads = true;
}

TEST_F(Microcosm, RendersWithoutShadersToo) {
    // The non-shader path shifts texture coordinates by hand instead of
    // handing them to a uniform (microcosm.cpp:634).
    stop();
    dShaders = 0;
    start();
    draw();

    EXPECT_EQ(glstub::trace().countCalls("glUseProgramObjectARB"), 0);
    EXPECT_GT(glstub::trace().totalArrayVertices(), 0u);
    EXPECT_TRUE(savertest::MatrixStackBalanced());
}

// --- settings change what is drawn -----------------------------------------

TEST_F(Microcosm, BothModesDrawCoherentFrames) {
    // gMode 0 centres one gizmo; gMode 1 tiles it through a mirror box over a
    // (2 * dDepth + 1) cubed grid (microcosm.cpp:716-729). No assertion on the
    // relative amount drawn: the tiling is culled against the view volume, and
    // the compiled-out clip planes (mirrorBox.h:28) mean the two paths can
    // legitimately issue the same number of draws.
    start();
    draw();
    EXPECT_GT(glstub::trace().countCalls("glDrawElements"), 0) << "single mode";
    EXPECT_TRUE(savertest::MatrixStackBalanced()) << "single mode";
    EXPECT_TRUE(savertest::PrimitivesPaired()) << "single mode";

    gMode = 1;
    glstub::reset();
    draw();
    EXPECT_GT(glstub::trace().countCalls("glDrawElements"), 0) << "kaleidoscope mode";
    EXPECT_TRUE(savertest::MatrixStackBalanced()) << "kaleidoscope mode";
    EXPECT_TRUE(savertest::PrimitivesPaired()) << "kaleidoscope mode";
    EXPECT_TRUE(savertest::NoInvalidEnums()) << "kaleidoscope mode";

    gMode = 0;
}

TEST_F(Microcosm, FogIsSetUpOnlyWhenAskedFor) {
    // Unlike lattice, microcosm configures fog inside the frame rather than at
    // setup (microcosm.cpp:660-662), because the range depends on the camera.
    stop();
    dFog = 0;
    start();
    draw();
    EXPECT_EQ(glstub::trace().countCalls("glFogf"), 0);

    stop();
    dFog = 1;
    start();
    draw();
    EXPECT_GT(glstub::trace().countCalls("glFogf"), 0);
}

TEST_F(Microcosm, EveryGizmoDrawsCoherently) {
    // The gizmos share one draw path but build very different implicit
    // geometry - metaballs, knots, tori, flowers, mirror-box kaleidoscopes -
    // and the suite would otherwise reach whichever one initSaver happened to
    // pick at random.
    //
    // The count comes from the list rather than being written down here, so
    // adding a gizmo extends this test rather than silently escaping it.
    start();
    ASSERT_GT(gizmos.size(), 1u) << "initSaver builds the list";

    for (size_t index = 0; index < gizmos.size(); ++index) {
        chooseGizmo(static_cast<int>(index));
        ASSERT_EQ(gGizmoIndex, index) << "chooseGizmo must actually select it";

        glstub::reset();
        draw();

        EXPECT_TRUE(savertest::PrimitivesPaired()) << "gizmo " << index;
        EXPECT_TRUE(savertest::VertexCountsLegal()) << "gizmo " << index;
        EXPECT_TRUE(savertest::MatrixStackBalanced()) << "gizmo " << index;
        EXPECT_TRUE(savertest::NoInvalidEnums()) << "gizmo " << index;
    }
}

TEST_F(Microcosm, GizmoListRebuiltOnEveryRestart) {
    // Was pinned as a defect: initSaver appended its 55 gizmos to whatever was
    // already there, because the clear that should precede them was swallowed by
    // the comment on the same line -
    //
    //     // initialize gizmos    gizmos.clear();
    //
    // and cleanUp freed nothing, so a second start left 110 entries and 55
    // leaked Gizmo objects. Now cleanUp deletes the list and initSaver rebuilds
    // it, so the size is the same on every start.
    start();
    const size_t first = gizmos.size();
    ASSERT_GT(first, 1u) << "initSaver builds the list";

    stop();
    EXPECT_TRUE(gizmos.empty()) << "cleanUp owns the list it is handed";

    start();
    const size_t second = gizmos.size();
    stop();
    start();
    const size_t third = gizmos.size();

    EXPECT_EQ(second, first);
    EXPECT_EQ(third, first);
}

TEST_F(Microcosm, EasterEggStaysHiddenAcrossRestarts) {
    // chooseGizmo hides the Tennis gizmo - the last entry - by dropping one off
    // the top of its random range while gTennisAvailable is false
    // (microcosm.cpp:371). With the list doubled, that guard still excluded only
    // the final entry, so the first copy's Tennis became reachable at random.
    // This is the one visible consequence of the append bug, so it gets its own
    // case rather than riding on the size assertion above.
    start();
    stop();
    start();

    // Set here rather than trusted from cleanUp: the flag also flips on elapsed
    // time inside draw(), and this case should not depend on how much time
    // earlier cases drove.
    gTennisAvailable = false;

    const size_t tennis = gizmos.size() - 1;
    for (int i = 0; i < 200; ++i) {
        chooseGizmo(-1);  // the random path; the default argument is on the definition
        ASSERT_NE(static_cast<size_t>(gGizmoIndex), tennis)
            << "easter egg drawn at random on iteration " << i;
    }
}

TEST_F(Microcosm, GizmoDestructionIsVirtual) {
    // cleanUp deletes the gizmos through Gizmo*, so ~Gizmo has to be virtual and
    // has to be the one place mShapes is freed. The subclasses used to free
    // mShapes themselves; if any of those destructors comes back, this is a
    // double free rather than a leak.
    static_assert(std::has_virtual_destructor<Gizmo>::value,
                  "cleanUp deletes gizmos through a Gizmo*");

    // One per ownership shape. A double free trips the debug heap here, in a
    // named case, instead of anonymously inside some other case's teardown.
    //
    // The debug CRT also answers the other half of the question - whether the
    // shapes are freed at all, which no assertion about the vector can show.
    // Only the four constructions below run between the checkpoints.
#ifdef _DEBUG
    _CrtMemState before;
    _CrtMemCheckpoint(&before);
#endif

    delete static_cast<Gizmo*>(new Cube);
    delete static_cast<Gizmo*>(new Orbit);
    delete static_cast<Gizmo*>(new Brain(4));
    delete static_cast<Gizmo*>(new RingOfTori(3));

#ifdef _DEBUG
    _CrtMemState after;
    _CrtMemCheckpoint(&after);
    _CrtMemState diff;
    EXPECT_EQ(_CrtMemDifference(&diff, &before, &after), 0)
        << "a gizmo destroyed through Gizmo* left memory behind";
#endif
}

// --- framework entry points ------------------------------------------------

TEST_F(Microcosm, IdleProcSkipsDrawingWhenNotReady) {
    start();
    readyToDraw = 0;
    glstub::reset();

    idleProc();

    EXPECT_EQ(glstub::trace().countCalls("glDrawElements"), 0);
    readyToDraw = 1;
}

TEST(MicrocosmFramework, ScreenSaverProcInitialisesOnCreateAndTearsDownOnDestroy) {
    setDefaults(0);
    gUseThreads = false;
    doingPreview = 1;
    readyToDraw = 0;

    screenSaverProc(testsupport::hostWindow(), WM_CREATE, 0, 0);
    EXPECT_EQ(readyToDraw, 1);

    screenSaverProc(testsupport::hostWindow(), WM_DESTROY, 0, 0);
    EXPECT_EQ(readyToDraw, 0);

    doingPreview = 0;
    gUseThreads = true;
}

TEST(MicrocosmFramework, ReadRegistryLeavesEverySettingInsideItsDeclaredRange) {
    // Read-only: it calls setDefaults(0) first and returns early if the key is
    // absent, so this cannot disturb the machine. That early return also means
    // the clamp itself covers little where the saver has never stored settings,
    // CI included - see the note in test_cyclone.cpp.
    readRegistry();

    EXPECT_GT(dResolution, 0) << "the implicit volume is sized from this";
    EXPECT_GT(dDepth, 0);
    EXPECT_GT(dFov, 0);
    EXPECT_TRUE(savertest::SettingsWithinDeclaredRanges(declaredRanges()));
}

// --- dialog procedures -----------------------------------------------------

TEST(MicrocosmDialogs, AboutProcColoursTheWebPageLabel) {
    EXPECT_TRUE(savertest::AboutProcColoursTheWebPageLabel(aboutProc));
}

TEST(MicrocosmDialogs, AboutProcIgnoresMessagesItDoesNotHandle) {
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(aboutProc));
}

TEST(MicrocosmDialogs, InitControlsRunsWithoutADialog) {
    setDefaults(0);
    EXPECT_NO_FATAL_FAILURE(initControls(nullptr));
}

TEST(MicrocosmDialogs, ConfigureDialogHandlesTheStandardMessages) {
    EXPECT_TRUE(savertest::ConfigureDialogInitialisesAndCancels(screenSaverConfigureDialog));
    EXPECT_TRUE(savertest::IgnoresUnhandledMessages(screenSaverConfigureDialog));
}

TEST(MicrocosmDialogs, EveryPresetButtonRestoresItsPreset) {
    // The three buttons map to setDefaults(0), (1) and (2)
    // (microcosm.cpp:1384-1395) rather than passing their own control id.
    constexpr std::array buttons = {DEFAULTS0, DEFAULTS1, DEFAULTS2};
    for (int i = 0; i < 3; ++i) {
        setDefaults(i);
        const int expected = dResolution;
        dResolution = 99;

        screenSaverConfigureDialog(nullptr, WM_COMMAND, buttons[i], 0);

        EXPECT_EQ(dResolution, expected) << "preset button " << i;
    }
}
