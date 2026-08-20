/*
 * Copyright (C) 1999-2010  Terence M. Welsh
 *
 * This file is part of Starfield.
 *
 * Starfield is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Starfield is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// Settings ranges and conversions for Starfield.
//
// Deliberately free of windows.h so it can be unit tested on any platform.
// The dialog, the command line and the registry all read their bounds from
// here, so the three cannot drift apart.

#ifndef STARFIELD_SETTINGS_H
#define STARFIELD_SETTINGS_H

#include "../common/saverSettings.h"

namespace starfieldSettings {

// Re-exported, never redefined. The bodies live once in rssaver; without
// these three lines a qualified call such as starfieldSettings::clampToRange
// does not compile, because argument-dependent lookup does not apply to
// qualified names and there is nothing of that name in this namespace.
using rssaver::Range;
using rssaver::clampToRange;
using rssaver::clampIntToRange;

constexpr Range kNumStars  = { 100, 10000 };
constexpr Range kSpeed     = { 1, 100 };
constexpr Range kStarSize  = { 1, 10 };
constexpr Range kFrameRate = { 1, 1000 };  // 0 means unlimited, handled separately

// Values applied by setDefaults() and by the Reset to defaults button
constexpr int kDefaultNumStars = 5000;
constexpr int kDefaultSpeed    = 5;
constexpr int kDefaultStarSize = 2;

// Offered in the FPS field when the limit is switched off
constexpr int kDefaultFrameRate = 60;


// The stored frame rate limit keeps its original meaning, where 0 is
// unlimited, but the dialog presents that as a checkbox plus an FPS value so
// the user never has to know 0 is special.
struct FrameRateUi { bool limited; int fps; };

inline FrameRateUi frameRateToUi(unsigned int stored){
	FrameRateUi ui;
	ui.limited = (stored != 0);
	// When unlimited, still offer a sensible number in the disabled field
	ui.fps = ui.limited ? clampToRange(stored, kFrameRate) : kDefaultFrameRate;
	return ui;
}

inline unsigned int frameRateFromUi(bool limited, int fps){
	if(!limited)
		return 0;
	// Clamping keeps a checked box from ever producing 0, which would silently
	// mean "unlimited" instead of "limited to the lowest rate"
	return (unsigned int)clampToRange(fps < 0 ? 0UL : (unsigned long)fps, kFrameRate);
}

}

#endif
