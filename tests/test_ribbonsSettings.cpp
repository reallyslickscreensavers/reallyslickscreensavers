/*
 * Tests for ribbonsSettings.h (ranges, presets, clampToRange)
 *
 * Suite names are prefixed because this file shares a binary with
 * test_starfieldSettings.cpp, and GoogleTest rejects two tests with the same
 * suite and case name.
 */

#include <gtest/gtest.h>
#include <ribbonsSettings.h>

using namespace ribbons;

// ---------------------------------------------------------------------------
// Range constants
//
// These pin the values the dialog controls, the command line and the registry
// all share.  They are asserted so that "preserve existing ranges" is enforced
// rather than left to review.
// ---------------------------------------------------------------------------

TEST(RibbonsRanges, MatchDocumentedValues) {
    EXPECT_EQ(kRibbonCount.lo, 1);
    EXPECT_EQ(kRibbonCount.hi, 10);
    EXPECT_EQ(kRibbonLength.lo, 10);
    EXPECT_EQ(kRibbonLength.hi, 200);
    EXPECT_EQ(kRibbonWidth.lo, 1);
    EXPECT_EQ(kRibbonWidth.hi, 100);
    EXPECT_EQ(kSpeed.lo, 1);
    EXPECT_EQ(kSpeed.hi, 100);
    EXPECT_EQ(kColorCycling.lo, 1);
    EXPECT_EQ(kColorCycling.hi, 100);
    EXPECT_EQ(kTransparency.lo, 1);
    EXPECT_EQ(kTransparency.hi, 100);
    EXPECT_EQ(kDefaultFrameRateLimit, 60u);
}

TEST(RibbonsRanges, AreOrderedLowToHigh) {
    // A reversed range would make clampToRange return the low bound for every
    // input, which reads as "the setting does nothing" rather than as a bug.
    EXPECT_LT(kRibbonCount.lo, kRibbonCount.hi);
    EXPECT_LT(kRibbonLength.lo, kRibbonLength.hi);
    EXPECT_LT(kRibbonWidth.lo, kRibbonWidth.hi);
    EXPECT_LT(kSpeed.lo, kSpeed.hi);
    EXPECT_LT(kColorCycling.lo, kColorCycling.hi);
    EXPECT_LT(kTransparency.lo, kTransparency.hi);
}

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------

TEST(RibbonsPresets, MatchDocumentedValues) {
    EXPECT_EQ(kGentle.ribbonCount, 3);
    EXPECT_EQ(kGentle.ribbonLength, 150);
    EXPECT_EQ(kVivid.ribbonCount, 5);
    EXPECT_EQ(kVivid.ribbonLength, 120);
    EXPECT_EQ(kChaos.ribbonCount, 7);
    EXPECT_EQ(kChaos.ribbonLength, 90);
}

// A preset outside its own control range would be silently clamped the first
// time the dialog opened, so the shipped default would not be what the user
// actually got.
TEST(RibbonsPresets, LieWithinTheirRanges) {
    const Preset presets[] = { kGentle, kVivid, kChaos };
    for (const Preset& p : presets) {
        EXPECT_GE(p.ribbonCount, kRibbonCount.lo);
        EXPECT_LE(p.ribbonCount, kRibbonCount.hi);
        EXPECT_GE(p.ribbonLength, kRibbonLength.lo);
        EXPECT_LE(p.ribbonLength, kRibbonLength.hi);
        EXPECT_GE(p.ribbonWidth, kRibbonWidth.lo);
        EXPECT_LE(p.ribbonWidth, kRibbonWidth.hi);
        EXPECT_GE(p.speed, kSpeed.lo);
        EXPECT_LE(p.speed, kSpeed.hi);
        EXPECT_GE(p.colorCycling, kColorCycling.lo);
        EXPECT_LE(p.colorCycling, kColorCycling.hi);
        EXPECT_GE(p.transparency, kTransparency.lo);
        EXPECT_LE(p.transparency, kTransparency.hi);
    }
}

// ---------------------------------------------------------------------------
// clampToRange
// ---------------------------------------------------------------------------

TEST(RibbonsClampToRange, LeavesValuesInsideTheRangeAlone) {
    EXPECT_EQ(clampToRange(5, kRibbonCount), 5);
    EXPECT_EQ(clampToRange(100, kRibbonLength), 100);
    EXPECT_EQ(clampToRange(50, kTransparency), 50);
}

TEST(RibbonsClampToRange, KeepsTheBoundsThemselves) {
    EXPECT_EQ(clampToRange(kRibbonCount.lo, kRibbonCount), kRibbonCount.lo);
    EXPECT_EQ(clampToRange(kRibbonCount.hi, kRibbonCount), kRibbonCount.hi);
    EXPECT_EQ(clampToRange(kRibbonLength.lo, kRibbonLength), kRibbonLength.lo);
    EXPECT_EQ(clampToRange(kRibbonLength.hi, kRibbonLength), kRibbonLength.hi);
}

TEST(RibbonsClampToRange, RaisesValuesBelowTheRange) {
    EXPECT_EQ(clampToRange(0, kRibbonCount), kRibbonCount.lo);
    EXPECT_EQ(clampToRange(3, kRibbonLength), kRibbonLength.lo);
}

TEST(RibbonsClampToRange, LowersValuesAboveTheRange) {
    EXPECT_EQ(clampToRange(999, kRibbonCount), kRibbonCount.hi);
    EXPECT_EQ(clampToRange(100000, kRibbonLength), kRibbonLength.hi);
}

// The reason the parameter is unsigned long rather than int: registry values
// arrive as DWORD, and converting first would turn 0xFFFFFFFF into -1, which a
// naive lower-bound check would raise to the minimum instead of the maximum.
TEST(RibbonsClampToRange, HandlesValuesThatWouldOverflowAnInt) {
    EXPECT_EQ(clampToRange(0x7FFFFFFFul, kRibbonCount), kRibbonCount.hi);
    EXPECT_EQ(clampToRange(0x80000000ul, kRibbonCount), kRibbonCount.hi);
    EXPECT_EQ(clampToRange(0xFFFFFFFFul, kRibbonCount), kRibbonCount.hi);
    EXPECT_EQ(clampToRange(0xFFFFFFFFul, kTransparency), kTransparency.hi);
}
