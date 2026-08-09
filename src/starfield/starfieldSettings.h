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

namespace starfield {

struct Range { int lo; int hi; };

const Range kNumStars  = { 100, 10000 };
const Range kSpeed     = { 1, 100 };
const Range kStarSize  = { 1, 10 };
const Range kFrameRate = { 1, 1000 };  // 0 means unlimited, handled separately

const int kDefaultFrameRate = 60;


// Clamp an untrusted value into range.  Takes unsigned long because registry
// values arrive as DWORD; converting to int first would turn 0xFFFFFFFF into
// -1 and slip past a naive lower-bound check.
inline int clampToRange(unsigned long v, Range r){
	if(v > (unsigned long)r.hi) return r.hi;
	if((int)v < r.lo) return r.lo;
	return (int)v;
}


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
