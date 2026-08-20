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

#include <array>
#include <vector>

// The particle types, the init/pop functions and addParticle. skyrocket.cpp
// includes this alongside resource.h already, so the two agree on macros.
#include "particle.h"

#include "resource.h"
#include "skyrocketSettings.h"

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

// The pool addParticle hands out of. draw() is what grows it, so a test must
// have drawn at least once before it calls addParticle - and must not hold a
// pointer OR an index into it across a draw(), because that call both resizes
// the vector (skyrocket.cpp:660-662) and copies the last particle over any
// removed slot (skyrocket.cpp:149-157).
extern std::vector<particle> particles;

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

// addParticle hands back a recycled slot, and particle::particle()
// (particle.cpp:27-42) sets type, displayList, drag, t, tr, bright, life, size,
// makeSmoke, smokeTimeIndex, smokeTrailLength, sparkTrailLength and depth -
// but NOT xyz, lastxyz, vel, rgb, explosiontype, thrust, endthrust, spin, tilt
// or tiltvec. In Debug those read back as the 0xCC fill, about -1.07e8, and
// draw() removes any particle whose xyz[1] is below zero (skyrocket.cpp:765),
// so an unplaced particle is deleted before anything runs on it and every
// assertion about it passes vacuously.
//
// Call this BOTH before and after the init* call. Before, because the four
// spawning initialisers - initSucker, initShockwave, initStretcher, initBigmama
// - copy this particle's xyz and vel into their 180 to 240 children. After,
// because initRocket and initFountain randomise xyz themselves.
//
// The values:
//   xyz      (0, altitude, 0) - the caller's altitude, above ground so the
//            removal check at skyrocket.cpp:765 does not delete it
//   lastxyz  = xyz, so the first frame's trail lengths are vel * frameTime
//            rather than a jump from a recycled slot's position
//   vel      (0, 100, 0) - NOT zero, and not an arbitrary non-zero either: it
//            is exactly what initRocket sets (particle.cpp:91). update()'s
//            ROCKET arm does dir = vel; dir.normalize() (particle.cpp:1084-1085)
//            under a branch this helper guarantees is taken, since it sets life
//            to 1.0f and initRocket leaves endthrust in [0.3, 0.4] (:99).
//            Matching initRocket means the after-place() call cannot change the
//            rocket's flight at all. SPINNER is not affected either way - its
//            block normalises a cross product with tiltvec from a fixed
//            dir.set(1, 0, 0) (:1290-1292), never vel.
//   rgb      (1.0, 0.8, 0.6) - any in-range colour; nothing asserts on it
//   life     1.0f  and bright 1.0f - alive and visible, so particle::draw()
//            does not early-return at particle.cpp:1417
//   t, tr    2.0f both, equal - the tr < t movement guard in 1b depends on
//            them starting equal
//   depth    1000.0f - particle::draw() culls on depth < 0 (:1421)
//
// Of the ten fields the constructor leaves alone, this sets six. The other four
// are deliberate:
//   thrust, endthrust, spin, tilt, tiltvec are read by update() only under
//   type == ROCKET (particle.cpp:1083-1094), type == BEE (:1095-1098 and
//   :1164-1166) and type == SPINNER (:1289-1313). initRocket sets all five
//   (:98-102), initSpinner sets spin, tilt and tiltvec (:149-152), initBee sets
//   thrust, endthrust, spin and tiltvec (:273-276) and BEE never reads tilt.
//   Overwriting them after the initialiser would flatten the thrust path.
//   explosiontype is read only under type == ROCKET and type == POPPER
//   (skyrocket.cpp:773, :779), and initRocket (:112) and the four POPPER
//   initialisers (:222, :235, :247, :259) all set it. Leaving it out is what
//   lets case 1a own that field for its sweep.
void place(particle* p, float altitude) {
    p->xyz.set(0.0f, altitude, 0.0f);
    p->lastxyz = p->xyz;
    p->vel.set(0.0f, 100.0f, 0.0f);
    p->rgb.set(1.0f, 0.8f, 0.6f);
    p->life = 1.0f;
    p->bright = 1.0f;
    p->t = p->tr = 2.0f;
    p->depth = 1000.0f;
}

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

TEST(SkyrocketHarness, DefaultsSitInsideTheDeclaredRanges) {
    // The header declares the ranges and the saver picks the defaults; nothing
    // else checks that the two agree.
    setDefaults();
    EXPECT_TRUE(savertest::SettingsWithinDeclaredRanges({
        savertest::Ranged("dMaxrockets", dMaxrockets, skyrocketSettings::kMaxrockets),
        savertest::Ranged("dSmoke", dSmoke, skyrocketSettings::kSmoke),
        savertest::Ranged("dExplosionsmoke", dExplosionsmoke, skyrocketSettings::kExplosionsmoke),
        savertest::Ranged("dWind", dWind, skyrocketSettings::kWind),
        savertest::Ranged("dAmbient", dAmbient, skyrocketSettings::kAmbient),
        savertest::Ranged("dStardensity", dStardensity, skyrocketSettings::kStardensity),
        savertest::Ranged("dFlare", dFlare, skyrocketSettings::kFlare),
        savertest::Ranged("dMoonglow", dMoonglow, skyrocketSettings::kMoonglow),
        savertest::Ranged("dSound", dSound, skyrocketSettings::kSound),
        savertest::Ranged("dMoon", dMoon, skyrocketSettings::kMoon),
        savertest::Ranged("dClouds", dClouds, skyrocketSettings::kClouds),
        savertest::Ranged("dEarth", dEarth, skyrocketSettings::kEarth),
        savertest::Ranged("dIllumination", dIllumination, skyrocketSettings::kIllumination),
    }));
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

// --- particle types, driven directly -----------------------------------
//
// These cases must stay above the sound section. cleanup() deletes
// soundengine without nulling it (skyrocket.cpp:978-980), and every init* and
// initExplosion in particle.cpp tests if(soundengine) - so a case that runs
// after DrivesTheSoundEngineWhenAsked in the same process reads a freed
// pointer. ctest forks per test and hides it; running the binary directly
// does not.

TEST_F(Skyrocket, EveryExplosionTypeSpawnsTheParticlesItPromises) {
    dMaxrockets = 1;   // never 0: draw() holds `static float rocketTimeConst =
                       // 10.0f / float(dMaxrockets)`, initialised once per process,
                       // and inf there stops every later case in a direct run from
                       // ever launching.
    dSmoke = 0;        // bounds the population; rocket smoke is already covered by
                       // KeepsDrawingCoherentlyWhileRocketsFly.
    dMoon = dClouds = dEarth = dStardensity = 0;   // the world is not what is under test
    start();

    // Every explosiontype the switch at particle.cpp:705 handles: 0-20 plus
    // the three "little explosion" codes poppers rewrite themselves to.
    std::vector<int> types;
    for (int t = 0; t <= 20; ++t) types.push_back(t);
    types.push_back(100);
    types.push_back(101);
    types.push_back(102);

    for (int type : types) {
        glstub::reset();
        particle* p = addParticle();
        place(p, 1000.0f);          // BEFORE: initSucker and initStretcher copy this into their children
        p->initRocket();
        place(p, 1000.0f);          // AFTER: initRocket randomises xyz and puts y at 5
        p->explosiontype = type;    // strictly between the after-place() and initExplosion():
                                     // place() must not own this field, and if a later edit
                                     // makes it own it again, this assignment still wins
        const unsigned int before = last_particle;
        p->initExplosion();

        // 17 is a flash only and 18 is a spinner not present in the switch
        // (particle.cpp:794-798) - both spawn nothing by design.
        if (type != 17 && type != 18) {
            EXPECT_GT(last_particle, before) << "explosiontype " << type;
        }

        // frameTime must be set inside the loop, before each draw() - it is a
        // global that only idleProc otherwise writes, and at zero the frame
        // simulates nothing.
        frameTime = kFrameSeconds;
        draw();

        EXPECT_TRUE(savertest::PrimitivesPaired());
        EXPECT_TRUE(savertest::VertexCountsLegal());
        EXPECT_TRUE(savertest::MatrixStackBalanced());
        EXPECT_TRUE(savertest::NoInvalidEnums());
    }
}

TEST_F(Skyrocket, EveryParticleTypeUpdatesAndDrawsCoherently) {
    dMaxrockets = 1;
    dSmoke = 0;
    dMoon = dClouds = dEarth = dStardensity = 0;
    start();

    constexpr float kAltitude = 1000.0f;

    // Phase 1: update. One particle per type, via its own initialiser, each
    // wrapped in place() before and after. initShockwave and initBigmama are
    // the payload here - the simulation only reaches them when a SUCKER or
    // STRETCHER dies, which needs the 1-in-2500 branch at skyrocket.cpp:702.
    // A shared altitude is fine: pulling, pushing and stretching all skip a
    // zero distance (skyrocket.cpp:274, :295, :316).
    auto spawn = [&](auto init) {
        particle* q = addParticle();
        place(q, kAltitude);
        (q->*init)();
        place(q, kAltitude);
    };

    spawn(&particle::initRocket);
    spawn(&particle::initFountain);
    spawn(&particle::initSpinner);
    spawn(&particle::initStar);
    spawn(&particle::initStreamer);
    spawn(&particle::initMeteor);
    spawn(&particle::initStarPopper);
    spawn(&particle::initStreamerPopper);
    spawn(&particle::initMeteorPopper);
    spawn(&particle::initLittlePopper);
    spawn(&particle::initBee);
    spawn(&particle::initSucker);
    spawn(&particle::initShockwave);
    spawn(&particle::initStretcher);
    spawn(&particle::initBigmama);

    {
        // initSmoke takes pos/vel rather than reading them off the particle
        // itself, so it does not fit the spawn() lambda above.
        particle* q = addParticle();
        place(q, kAltitude);
        q->initSmoke(q->xyz, q->vel);
        place(q, kAltitude);
    }

    for (int frame = 0; frame < 3; ++frame) {
        frameTime = kFrameSeconds;
        draw();
    }

    EXPECT_TRUE(savertest::MatrixStackBalanced());
    EXPECT_TRUE(savertest::PrimitivesPaired());
    EXPECT_TRUE(savertest::VertexCountsLegal());
    EXPECT_TRUE(savertest::NoInvalidEnums());

    // Both scans hold nothing across a frame on purpose. The first frame
    // resizes the pool and moves every element, and removeParticle copies the
    // last particle over any removed slot, so a pointer or an index captured
    // before the frames means nothing after them.
    int aged = 0;
    for (unsigned int i = 0; i < last_particle; ++i) {
        // Every initialiser and place() leave t == tr, and update() does
        // tr -= frameTime for every type before its switch (particle.cpp:1118),
        // so tr < t means, and only means, that a particle was updated with a
        // clock that was actually running.
        if (particles[i].tr < particles[i].t) ++aged;

        // The four shared invariants are all structural: a particle whose
        // position went to NaN still pushes and pops matrices, still pairs its
        // begins and ends, and survives the removal check at
        // skyrocket.cpp:765 because NaN < 0.0f is false. Nothing else here
        // would notice, so this is what notices. Only xyz is checked here -
        // bright, size, rgb and tiltvec are not.
        for (int axis = 0; axis < 3; ++axis)
            ASSERT_TRUE(std::isfinite(particles[i].xyz[axis]))
                << "particle " << i << " type " << particles[i].type
                << " went non-finite on axis " << axis;
    }
    EXPECT_GT(aged, 0) << "nothing aged - is frameTime being driven?";

    // Phase 2: draw. particle::draw() early-returns on life <= 0, on
    // depth < 0 for everything but SHOCKWAVE, and on POPPER
    // (particle.cpp:1413-1422), and the saver's own loop recomputes depth
    // from the camera each frame - so whether a given type reaches its switch
    // arm through the saver's draw() is a property of where the camera
    // happens to be. Do not leave it to that: create a fresh driver per type
    // again and call particle::draw() directly, immediately, before any
    // saver draw() can move the vector.
    struct Driver {
        const char* name;
        particle* p;
        bool issuesCallList;
    };
    std::vector<Driver> drivers;

    auto makeDriver = [&](auto init, const char* name, bool issuesCallList) {
        particle* q = addParticle();
        place(q, kAltitude);
        (q->*init)();
        place(q, kAltitude);
        drivers.push_back({name, q, issuesCallList});
    };

    makeDriver(&particle::initRocket, "ROCKET", true);
    makeDriver(&particle::initFountain, "FOUNTAIN", true);
    makeDriver(&particle::initSpinner, "SPINNER", true);
    makeDriver(&particle::initStar, "STAR", true);
    makeDriver(&particle::initStreamer, "STREAMER", true);
    makeDriver(&particle::initMeteor, "METEOR", true);
    // The four POPPER initialisers are early-returned at particle.cpp:1425-1426
    // and issue no glCallList at all.
    makeDriver(&particle::initStarPopper, "POPPER (star)", false);
    makeDriver(&particle::initStreamerPopper, "POPPER (streamer)", false);
    makeDriver(&particle::initMeteorPopper, "POPPER (meteor)", false);
    makeDriver(&particle::initLittlePopper, "POPPER (little)", false);
    makeDriver(&particle::initBee, "BEE", true);
    makeDriver(&particle::initSucker, "SUCKER", true);
    makeDriver(&particle::initShockwave, "SHOCKWAVE", true);
    makeDriver(&particle::initStretcher, "STRETCHER", true);
    makeDriver(&particle::initBigmama, "BIGMAMA", true);

    {
        particle* q = addParticle();
        place(q, kAltitude);
        q->initSmoke(q->xyz, q->vel);
        place(q, kAltitude);
        drivers.push_back({"SMOKE", q, true});
    }
    {
        // None of the sixteen drivers above produces type == EXPLOSION, so its
        // arm at particle.cpp:1459 would otherwise stay cold. explosiontype 17
        // is flash-only (spawns nothing), which keeps this driver's set up
        // identical in shape to the others.
        particle* q = addParticle();
        place(q, kAltitude);
        q->explosiontype = 17;
        q->initExplosion();
        place(q, kAltitude);
        drivers.push_back({"EXPLOSION (flash)", q, true});
    }

    ASSERT_EQ(drivers.size(), 17u);

    // The check is per driver, not a total: the arms issue different counts -
    // SHOCKWAVE issues glCallList(flarelist[0]) twice plus
    // glCallList(flarelist[2]) under if(life > 0.7f) (3 here, since place()
    // sets life = 1.0f), SMOKE 1, EXPLOSION 1, default 2
    // (particle.cpp:1432-1476) - so a total across all 17 means nothing as a
    // sum.
    for (const auto& d : drivers) {
        glstub::reset();
        d.p->draw();
        EXPECT_TRUE(savertest::MatrixStackBalanced()) << "driver " << d.name;
        const int calls = glstub::trace().countCalls("glCallList");
        if (d.issuesCallList) {
            EXPECT_GT(calls, 0) << "driver " << d.name;
        } else {
            EXPECT_EQ(calls, 0) << "driver " << d.name;
        }
    }
}

TEST_F(Skyrocket, RandomColourAlwaysSetsExactlyOneChannelFull) {
    // Frozen: no draw(), so this claims no update() coverage. It pins the
    // invariant that makes particle.cpp:49 (the switch on rsRandi(6))
    // unreachable through a default: arm - rsRandi(6) returns [0, 6), and the
    // switch is total over that range - the way CycloneBlockerGuard pins the
    // reason the BLOCKERs are unreachable. This passes before and after the
    // fix in particle.cpp, by design.
    start();
    particle* p = addParticle();
    place(p, 1000.0f);

    for (int i = 0; i < 200; ++i) {
        rsVec color(0.0f, 0.0f, 0.0f);
        p->randomColor(color);

        int fullChannels = 0;
        for (int channel = 0; channel < 3; ++channel) {
            EXPECT_GE(color[channel], 0.0f);
            EXPECT_LE(color[channel], 1.0f);
            if (color[channel] == 1.0f) ++fullChannels;
        }
        EXPECT_EQ(fullChannels, 1);
    }
}

TEST_F(Skyrocket, PopFunctionsStillGrantTheOccasionalLongLife) {
    // Frozen: no draw(), so this claims no update() coverage either.
    //
    // This is the behaviour-preservation guard for the particle.cpp fix -
    // what fails if the null check there is written as
    // `if(newp == nullptr || !rsRandi(100))` or the branch is dropped - and it
    // is what deterministically covers the bodies at particle.cpp:844, :874,
    // :902 and :938, which the 1-in-100 branch otherwise leaves cold.
    //
    // Indexing the pool is safe HERE SPECIFICALLY because this case never
    // draws, so nothing resizes the vector and nothing is removed between the
    // call and the read below. Do not copy this pattern into a case that
    // draws.
    //
    // The generator is seeded to kTestSeed in SetUp, so 500 draws is
    // deterministic; 0.99^500 ~= 0.007 is the odds it needed to be luckier
    // than that. No saturation: start() leaves the pool at 1000 with
    // last_particle in the low tens.
    start();
    particle* p = addParticle();
    place(p, 1000.0f);

    rsVec colour(1.0f, 0.8f, 0.6f);
    bool sawLongLife = false;
    for (int i = 0; i < 500; ++i) {
        p->popSphere(1, 100.0f, colour);
        if (particles[last_particle - 1].t > 4.0f) {
            sawLongLife = true;
            break;
        }
    }
    EXPECT_TRUE(sawLongLife) << "the 1-in-100 long-life branch never fired in 500 tries";
}

TEST_F(Skyrocket, PopFunctionsAddNothingWhenAskedForNoParticles) {
    // Frozen: no draw(). No shipped call site passes a non-positive count -
    // see skyrocket.cpp and particle.cpp's own callers - so this is about the
    // four pop* functions' declared parameter domain rather than a reachable
    // crash. Written as one loop over the four calls rather than four
    // near-identical bodies, per the duplication gate.
    start();
    particle* p = addParticle();
    place(p, 1000.0f);
    rsVec colour(1.0f, 0.8f, 0.6f);

    const unsigned int before = last_particle;
    for (int i = 0; i < 200; ++i) {
        p->popSphere(0, 100.0f, colour);
        p->popSplitSphere(0, 100.0f, colour);
        p->popMultiColorSphere(0, 100.0f);
        p->popRing(0, 100.0f, colour);
    }
    EXPECT_EQ(last_particle, before);
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

// Anything that touches particle.cpp's if(soundengine) paths must be declared
// ABOVE this case - it leaves soundengine dangling, see the note there. Do
// not move this case up.
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

TEST(SkyrocketFramework, ReadRegistryLeavesEverySettingInsideItsDeclaredRange) {
    // Read-only: setDefaults runs first and the function returns early if the
    // key is absent, so this cannot disturb the machine. That early return also
    // means the clamp itself covers little where the saver has never stored
    // settings, CI included - see the note in test_cyclone.cpp.
    setDefaults();
    readRegistry();

    EXPECT_GT(dMaxrockets, 0) << "the rocket array is allocated with this count";
    EXPECT_TRUE(savertest::SettingsWithinDeclaredRanges({
        savertest::Ranged("dMaxrockets", dMaxrockets, skyrocketSettings::kMaxrockets),
        savertest::Ranged("dSmoke", dSmoke, skyrocketSettings::kSmoke),
        savertest::Ranged("dExplosionsmoke", dExplosionsmoke, skyrocketSettings::kExplosionsmoke),
        savertest::Ranged("dWind", dWind, skyrocketSettings::kWind),
        savertest::Ranged("dAmbient", dAmbient, skyrocketSettings::kAmbient),
        savertest::Ranged("dStardensity", dStardensity, skyrocketSettings::kStardensity),
        savertest::Ranged("dFlare", dFlare, skyrocketSettings::kFlare),
        savertest::Ranged("dMoonglow", dMoonglow, skyrocketSettings::kMoonglow),
        savertest::Ranged("dSound", dSound, skyrocketSettings::kSound),
        savertest::Ranged("dMoon", dMoon, skyrocketSettings::kMoon),
        savertest::Ranged("dClouds", dClouds, skyrocketSettings::kClouds),
        savertest::Ranged("dEarth", dEarth, skyrocketSettings::kEarth),
        savertest::Ranged("dIllumination", dIllumination, skyrocketSettings::kIllumination),
    }));
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
