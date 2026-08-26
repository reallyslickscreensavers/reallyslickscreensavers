/*
 * Tests for src/common/saverSettings.h and every per-saver <name>Settings.h
 * (Task 11 in docs/MAINTENANCE.md).
 *
 * Header-only: no saver body, no windows.h. That this translation unit
 * compiles at all is itself part of the assertion - every one of the
 * thirteen settings headers has to be windows.h-free for that to hold.
 */

#include <gtest/gtest.h>

#include <array>
#include <climits>
#include <string>

#include <common/saverSettings.h>

#include <cyclone/cycloneSettings.h>
#include <euphoria/euphoriaSettings.h>
#include <fieldlines/fieldlinesSettings.h>
#include <flocks/flocksSettings.h>
#include <flux/fluxSettings.h>
#include <helios/heliosSettings.h>
#include <hyperspace/hyperspaceSettings.h>
#include <lattice/latticeSettings.h>
#include <microcosm/microcosmSettings.h>
#include <plasma/plasmaSettings.h>
#include <skyrocket/skyrocketSettings.h>
#include <solarwinds/solarWindsSettings.h>
#include <starfield/starfieldSettings.h>

namespace {

struct NamedRange {
    const char* saver;
    const char* setting;
    rssaver::Range range;
};

// cyclone declares its own Range rather than re-exporting rssaver::Range,
// because its settings header also carries the dialog's frame-rate conversions
// and predates the shared one (PR #62). The two are layout-identical, so its
// rows are widened explicitly here rather than by changing that header.
constexpr rssaver::Range Widen(cycloneSettings::Range r) {
    return rssaver::Range{r.lo, r.hi};
}

// All 112 saver-specific settings, one row per readRegistry clamp site.
//
// dFrameRateLimit is deliberately absent from every saver here - see the
// "Why dFrameRateLimit is not asserted per-saver" note in the per-saver
// suites. cyclone contributes 5 rather than 7: its two checkboxes go through
// cycloneSettings::normalizeFlag instead of a {0,1} Range, so they have no row
// to carry. tests/test_cyclone.cpp asserts them as flags instead.
constexpr std::array<NamedRange, 112> kAllSettings = {{
    // cyclone - 5
    {"cyclone", "kCyclones", Widen(cycloneSettings::kCyclones)},
    {"cyclone", "kParticles", Widen(cycloneSettings::kParticles)},
    {"cyclone", "kSize", Widen(cycloneSettings::kSize)},
    {"cyclone", "kComplexity", Widen(cycloneSettings::kComplexity)},
    {"cyclone", "kSpeed", Widen(cycloneSettings::kSpeed)},

    // euphoria - 10
    {"euphoria", "kWisps", euphoriaSettings::kWisps},
    {"euphoria", "kBackground", euphoriaSettings::kBackground},
    {"euphoria", "kDensity", euphoriaSettings::kDensity},
    {"euphoria", "kVisibility", euphoriaSettings::kVisibility},
    {"euphoria", "kSpeed", euphoriaSettings::kSpeed},
    {"euphoria", "kFeedback", euphoriaSettings::kFeedback},
    {"euphoria", "kFeedbackspeed", euphoriaSettings::kFeedbackspeed},
    {"euphoria", "kFeedbacksize", euphoriaSettings::kFeedbacksize},
    {"euphoria", "kTexture", euphoriaSettings::kTexture},
    {"euphoria", "kWireframe", euphoriaSettings::kWireframe},

    // fieldlines - 7
    {"fieldlines", "kIons", fieldlinesSettings::kIons},
    {"fieldlines", "kStepSize", fieldlinesSettings::kStepSize},
    {"fieldlines", "kMaxSteps", fieldlinesSettings::kMaxSteps},
    {"fieldlines", "kWidth", fieldlinesSettings::kWidth},
    {"fieldlines", "kSpeed", fieldlinesSettings::kSpeed},
    {"fieldlines", "kConstwidth", fieldlinesSettings::kConstwidth},
    {"fieldlines", "kElectric", fieldlinesSettings::kElectric},

    // flocks - 10
    {"flocks", "kLeaders", flocksSettings::kLeaders},
    {"flocks", "kFollowers", flocksSettings::kFollowers},
    {"flocks", "kGeometry", flocksSettings::kGeometry},
    {"flocks", "kSize", flocksSettings::kSize},
    {"flocks", "kComplexity", flocksSettings::kComplexity},
    {"flocks", "kSpeed", flocksSettings::kSpeed},
    {"flocks", "kStretch", flocksSettings::kStretch},
    {"flocks", "kColorfadespeed", flocksSettings::kColorfadespeed},
    {"flocks", "kChromatek", flocksSettings::kChromatek},
    {"flocks", "kConnections", flocksSettings::kConnections},

    // flux - 12
    {"flux", "kFluxes", fluxSettings::kFluxes},
    {"flux", "kParticles", fluxSettings::kParticles},
    {"flux", "kTrail", fluxSettings::kTrail},
    {"flux", "kGeometry", fluxSettings::kGeometry},
    {"flux", "kSize", fluxSettings::kSize},
    {"flux", "kComplexity", fluxSettings::kComplexity},
    {"flux", "kRandomize", fluxSettings::kRandomize},
    {"flux", "kExpansion", fluxSettings::kExpansion},
    {"flux", "kRotation", fluxSettings::kRotation},
    {"flux", "kWind", fluxSettings::kWind},
    {"flux", "kInstability", fluxSettings::kInstability},
    {"flux", "kBlur", fluxSettings::kBlur},

    // helios - 8
    {"helios", "kIons", heliosSettings::kIons},
    {"helios", "kSize", heliosSettings::kSize},
    {"helios", "kEmitters", heliosSettings::kEmitters},
    {"helios", "kAttracters", heliosSettings::kAttracters},
    {"helios", "kSpeed", heliosSettings::kSpeed},
    {"helios", "kCameraspeed", heliosSettings::kCameraspeed},
    {"helios", "kSurface", heliosSettings::kSurface},
    {"helios", "kBlur", heliosSettings::kBlur},

    // hyperspace - 9
    {"hyperspace", "kSpeed", hyperspaceSettings::kSpeed},
    {"hyperspace", "kStars", hyperspaceSettings::kStars},
    {"hyperspace", "kStarSize", hyperspaceSettings::kStarSize},
    {"hyperspace", "kResolution", hyperspaceSettings::kResolution},
    {"hyperspace", "kDepth", hyperspaceSettings::kDepth},
    {"hyperspace", "kFov", hyperspaceSettings::kFov},
    {"hyperspace", "kUseTunnels", hyperspaceSettings::kUseTunnels},
    {"hyperspace", "kUseGoo", hyperspaceSettings::kUseGoo},
    {"hyperspace", "kShaders", hyperspaceSettings::kShaders},

    // lattice - 11
    {"lattice", "kLongitude", latticeSettings::kLongitude},
    {"lattice", "kLatitude", latticeSettings::kLatitude},
    {"lattice", "kThick", latticeSettings::kThick},
    {"lattice", "kDensity", latticeSettings::kDensity},
    {"lattice", "kDepth", latticeSettings::kDepth},
    {"lattice", "kFov", latticeSettings::kFov},
    {"lattice", "kPathrand", latticeSettings::kPathrand},
    {"lattice", "kSpeed", latticeSettings::kSpeed},
    {"lattice", "kTexture", latticeSettings::kTexture},
    {"lattice", "kSmooth", latticeSettings::kSmooth},
    {"lattice", "kFog", latticeSettings::kFog},

    // microcosm - 11
    {"microcosm", "kKaleidoscopeTime", microcosmSettings::kKaleidoscopeTime},
    {"microcosm", "kSingleTime", microcosmSettings::kSingleTime},
    {"microcosm", "kBackground", microcosmSettings::kBackground},
    {"microcosm", "kResolution", microcosmSettings::kResolution},
    {"microcosm", "kDepth", microcosmSettings::kDepth},
    {"microcosm", "kFov", microcosmSettings::kFov},
    {"microcosm", "kGizmoSpeed", microcosmSettings::kGizmoSpeed},
    {"microcosm", "kColorSpeed", microcosmSettings::kColorSpeed},
    {"microcosm", "kCameraSpeed", microcosmSettings::kCameraSpeed},
    {"microcosm", "kShaders", microcosmSettings::kShaders},
    {"microcosm", "kFog", microcosmSettings::kFog},

    // plasma - 4
    {"plasma", "kZoom", plasmaSettings::kZoom},
    {"plasma", "kFocus", plasmaSettings::kFocus},
    {"plasma", "kSpeed", plasmaSettings::kSpeed},
    {"plasma", "kResolution", plasmaSettings::kResolution},

    // skyrocket - 13
    {"skyrocket", "kMaxrockets", skyrocketSettings::kMaxrockets},
    {"skyrocket", "kSmoke", skyrocketSettings::kSmoke},
    {"skyrocket", "kExplosionsmoke", skyrocketSettings::kExplosionsmoke},
    {"skyrocket", "kWind", skyrocketSettings::kWind},
    {"skyrocket", "kAmbient", skyrocketSettings::kAmbient},
    {"skyrocket", "kStardensity", skyrocketSettings::kStardensity},
    {"skyrocket", "kFlare", skyrocketSettings::kFlare},
    {"skyrocket", "kMoonglow", skyrocketSettings::kMoonglow},
    {"skyrocket", "kSound", skyrocketSettings::kSound},
    {"skyrocket", "kMoon", skyrocketSettings::kMoon},
    {"skyrocket", "kClouds", skyrocketSettings::kClouds},
    {"skyrocket", "kEarth", skyrocketSettings::kEarth},
    {"skyrocket", "kIllumination", skyrocketSettings::kIllumination},

    // solarwinds - 9
    {"solarwinds", "kWinds", solarWindsSettings::kWinds},
    {"solarwinds", "kEmitters", solarWindsSettings::kEmitters},
    {"solarwinds", "kParticles", solarWindsSettings::kParticles},
    {"solarwinds", "kGeometry", solarWindsSettings::kGeometry},
    {"solarwinds", "kSize", solarWindsSettings::kSize},
    {"solarwinds", "kWindspeed", solarWindsSettings::kWindspeed},
    {"solarwinds", "kEmitterspeed", solarWindsSettings::kEmitterspeed},
    {"solarwinds", "kParticlespeed", solarWindsSettings::kParticlespeed},
    {"solarwinds", "kBlur", solarWindsSettings::kBlur},

    // starfield - 3
    {"starfield", "kNumStars", starfieldSettings::kNumStars},
    {"starfield", "kSpeed", starfieldSettings::kSpeed},
    {"starfield", "kStarSize", starfieldSettings::kStarSize},
}};

constexpr int kExpectedTotal = 112;

int CountForSaver(const char* saver) {
    int n = 0;
    for (const auto& s : kAllSettings) {
        if (std::string(s.saver) == saver) ++n;
    }
    return n;
}

}  // namespace

TEST(SaverSettings, EveryRangeIsWellFormed) {
    for (const auto& s : kAllSettings) {
        EXPECT_LE(s.range.lo, s.range.hi)
            << s.saver << "::" << s.setting << " has lo > hi";
        EXPECT_GE(s.range.lo, 0)
            << s.saver << "::" << s.setting << " has a negative lower bound";
    }
}

// The trap the unsigned long parameter exists for: with an int parameter
// this would return lo instead, because 0xFFFFFFFF narrows to -1 first.
TEST(SaverSettings, ClampsAnOversizedDwordToTheMaximum) {
    for (const auto& s : kAllSettings) {
        EXPECT_EQ(rssaver::clampToRange(0xFFFFFFFFUL, s.range), s.range.hi)
            << s.saver << "::" << s.setting;
    }
}

TEST(SaverSettings, ClampsAtAndAroundBoundaries) {
    for (const auto& s : kAllSettings) {
        EXPECT_EQ(rssaver::clampToRange((unsigned long)s.range.lo, s.range), s.range.lo)
            << s.saver << "::" << s.setting;
        EXPECT_EQ(rssaver::clampToRange((unsigned long)s.range.hi, s.range), s.range.hi)
            << s.saver << "::" << s.setting;
        EXPECT_EQ(rssaver::clampToRange((unsigned long)s.range.hi + 1, s.range), s.range.hi)
            << s.saver << "::" << s.setting;
        if (s.range.lo > 0) {
            EXPECT_EQ(rssaver::clampToRange((unsigned long)(s.range.lo - 1), s.range), s.range.lo)
                << s.saver << "::" << s.setting;
        }
    }
}

TEST(SaverSettings, ClampIntToRangeSendsNegativesToTheLowBound) {
    for (const auto& s : kAllSettings) {
        EXPECT_EQ(rssaver::clampIntToRange(-1, s.range), s.range.lo)
            << s.saver << "::" << s.setting;
        EXPECT_EQ(rssaver::clampIntToRange(INT_MIN, s.range), s.range.lo)
            << s.saver << "::" << s.setting;
        EXPECT_EQ(rssaver::clampIntToRange(INT_MAX, s.range), s.range.hi)
            << s.saver << "::" << s.setting;
    }
}

TEST(SaverSettings, TableCoversEverySaversSettingCount) {
    EXPECT_EQ(CountForSaver("cyclone"), 5);  // 2 checkboxes use normalizeFlag
    EXPECT_EQ(CountForSaver("euphoria"), 10);
    EXPECT_EQ(CountForSaver("fieldlines"), 7);
    EXPECT_EQ(CountForSaver("flocks"), 10);
    EXPECT_EQ(CountForSaver("flux"), 12);
    EXPECT_EQ(CountForSaver("helios"), 8);
    EXPECT_EQ(CountForSaver("hyperspace"), 9);
    EXPECT_EQ(CountForSaver("lattice"), 11);
    EXPECT_EQ(CountForSaver("microcosm"), 11);
    EXPECT_EQ(CountForSaver("plasma"), 4);
    EXPECT_EQ(CountForSaver("skyrocket"), 13);
    EXPECT_EQ(CountForSaver("solarwinds"), 9);
    EXPECT_EQ(CountForSaver("starfield"), 3);

    EXPECT_EQ(static_cast<int>(kAllSettings.size()), kExpectedTotal);
}
