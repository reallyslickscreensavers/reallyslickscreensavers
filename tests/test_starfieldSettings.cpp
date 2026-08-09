/*
 * Tests for starfieldSettings.h (ranges, clampToRange, frame rate mapping)
 */

#include <gtest/gtest.h>
#include <starfieldSettings.h>

using namespace starfield;

// ---------------------------------------------------------------------------
// Range constants
//
// These pin the values the dialog sliders, the command line and the registry
// all share.  They are asserted so that "preserve existing ranges" is enforced
// rather than left to review.
// ---------------------------------------------------------------------------

TEST(Ranges, MatchDocumentedValues) {
    EXPECT_EQ(kNumStars.lo, 100);
    EXPECT_EQ(kNumStars.hi, 10000);
    EXPECT_EQ(kSpeed.lo, 1);
    EXPECT_EQ(kSpeed.hi, 100);
    EXPECT_EQ(kStarSize.lo, 1);
    EXPECT_EQ(kStarSize.hi, 10);
    EXPECT_EQ(kFrameRate.lo, 1);
    EXPECT_EQ(kFrameRate.hi, 1000);
    EXPECT_EQ(kDefaultFrameRate, 60);
}

// ---------------------------------------------------------------------------
// clampToRange
// ---------------------------------------------------------------------------

TEST(ClampToRange, PassesValuesInsideRange) {
    EXPECT_EQ(clampToRange(2000, kNumStars), 2000);
    EXPECT_EQ(clampToRange(50, kSpeed), 50);
    EXPECT_EQ(clampToRange(5, kStarSize), 5);
}

TEST(ClampToRange, ClampsAtBoundaries) {
    EXPECT_EQ(clampToRange(100, kNumStars), 100);
    EXPECT_EQ(clampToRange(10000, kNumStars), 10000);
    EXPECT_EQ(clampToRange(99, kNumStars), 100);
    EXPECT_EQ(clampToRange(10001, kNumStars), 10000);
}

TEST(ClampToRange, ZeroGoesToLowerBound) {
    EXPECT_EQ(clampToRange(0, kNumStars), 100);
    EXPECT_EQ(clampToRange(0, kStarSize), 1);
    EXPECT_EQ(clampToRange(0, kSpeed), 1);
}

// The reason the parameter is unsigned long rather than int: these values
// would all become negative if cast to int first, and slip past a naive
// lower-bound check instead of being clamped to the upper bound.
TEST(ClampToRange, LargeUnsignedValuesClampToUpperBound) {
    EXPECT_EQ(clampToRange(0x7FFFFFFFUL, kNumStars), 10000);  // INT_MAX
    EXPECT_EQ(clampToRange(0x80000000UL, kNumStars), 10000);  // INT_MAX + 1
    EXPECT_EQ(clampToRange(0xFFFFFFFFUL, kNumStars), 10000);  // would be -1
    EXPECT_EQ(clampToRange(0xFFFFFFFFUL, kStarSize), 10);
}

// ---------------------------------------------------------------------------
// frameRateToUi  (stored value -> dialog state)
// ---------------------------------------------------------------------------

TEST(FrameRateToUi, ZeroMeansUnlimited) {
    const FrameRateUi ui = frameRateToUi(0);
    EXPECT_FALSE(ui.limited);
    // Still offers a usable number in the disabled field
    EXPECT_EQ(ui.fps, kDefaultFrameRate);
}

TEST(FrameRateToUi, NonZeroMeansLimited) {
    const FrameRateUi ui = frameRateToUi(75);
    EXPECT_TRUE(ui.limited);
    EXPECT_EQ(ui.fps, 75);
}

TEST(FrameRateToUi, ClampsCorruptedStoredValue) {
    const FrameRateUi ui = frameRateToUi(5000);
    EXPECT_TRUE(ui.limited);
    EXPECT_EQ(ui.fps, kFrameRate.hi);
}

TEST(FrameRateToUi, AcceptsBoundaryValues) {
    EXPECT_EQ(frameRateToUi(1).fps, 1);
    EXPECT_EQ(frameRateToUi(1000).fps, 1000);
}

// ---------------------------------------------------------------------------
// frameRateFromUi  (dialog state -> stored value)
// ---------------------------------------------------------------------------

TEST(FrameRateFromUi, UncheckedStoresZeroWhateverTheFieldSays) {
    EXPECT_EQ(frameRateFromUi(false, 75), 0u);
    EXPECT_EQ(frameRateFromUi(false, 0), 0u);
    EXPECT_EQ(frameRateFromUi(false, 1000), 0u);
}

TEST(FrameRateFromUi, CheckedStoresTheValue) {
    EXPECT_EQ(frameRateFromUi(true, 75), 75u);
    EXPECT_EQ(frameRateFromUi(true, 1), 1u);
    EXPECT_EQ(frameRateFromUi(true, 1000), 1000u);
}

// A checked box must never produce 0, because 0 is how "unlimited" is stored.
TEST(FrameRateFromUi, CheckedNeverProducesZero) {
    EXPECT_EQ(frameRateFromUi(true, 0), (unsigned int)kFrameRate.lo);
    EXPECT_EQ(frameRateFromUi(true, -1), (unsigned int)kFrameRate.lo);
    EXPECT_EQ(frameRateFromUi(true, -99999), (unsigned int)kFrameRate.lo);
}

TEST(FrameRateFromUi, ClampsAboveRange) {
    EXPECT_EQ(frameRateFromUi(true, 5000), (unsigned int)kFrameRate.hi);
}

// ---------------------------------------------------------------------------
// Round trip
//
// This is the property that keeps the redesign backward compatible: the stored
// representation must survive a trip through the dialog untouched.
// ---------------------------------------------------------------------------

TEST(FrameRateRoundTrip, StoredValueSurvivesTheDialog) {
    const unsigned int values[] = { 0, 1, 30, 60, 144, 1000 };
    for (unsigned int v : values) {
        const FrameRateUi ui = frameRateToUi(v);
        EXPECT_EQ(frameRateFromUi(ui.limited, ui.fps), v)
            << "round trip failed for stored value " << v;
    }
}
