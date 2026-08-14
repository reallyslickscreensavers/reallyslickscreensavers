/*
 * Copyright (C) 1999-2010  Terence M. Welsh
 *
 * This file is part of Ribbons.
 *
 * Ribbons is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Ribbons is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// Settings ranges and presets for Ribbons.
//
// Deliberately free of windows.h so it can be unit tested on any platform.
// The dialog, the command line and the registry all read their bounds from
// here, so the three cannot drift apart.
//
// The frame rate limit is deliberately absent: it belongs to the framework,
// which owns both the slider and the clamp in
// <rsWin32Saver/rsWin32SaverSettings.h>.

#ifndef RIBBONS_SETTINGS_H
#define RIBBONS_SETTINGS_H

namespace ribbons {

struct Range { int lo; int hi; };

const Range kRibbonCount   = { 1, 10 };
const Range kRibbonLength  = { 10, 200 };
const Range kRibbonWidth   = { 1, 100 };
const Range kSpeed         = { 1, 100 };
const Range kColorCycling  = { 1, 100 };
const Range kTransparency  = { 1, 100 };


// The three presets behind the Gentle, Vivid and Chaos buttons.  setDefaults()
// applies one of these; every field must lie inside the ranges above, which is
// what test_ribbonsSettings asserts.
struct Preset {
	int ribbonCount;
	int ribbonLength;
	int ribbonWidth;
	int speed;
	int colorCycling;
	int transparency;
};

const Preset kGentle = { 3, 150, 40, 15, 25, 60 };
const Preset kVivid  = { 5, 120, 55, 30, 50, 75 };
const Preset kChaos  = { 7,  90, 35, 55, 80, 50 };

// Offered by every preset; the framework clamps it to its own range.
const unsigned int kDefaultFrameRateLimit = 60;


// Clamp an untrusted value into range.  Takes unsigned long because registry
// values arrive as DWORD; converting to int first would turn 0xFFFFFFFF into
// -1 and slip past a naive lower-bound check.
inline int clampToRange(unsigned long v, Range r){
	if(v > (unsigned long)r.hi) return r.hi;
	if((int)v < r.lo) return r.lo;
	return (int)v;
}

}

#endif
