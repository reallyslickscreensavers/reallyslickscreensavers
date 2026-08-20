/*
 * Copyright (C) 1999-2010  Terence M. Welsh
 *
 * This file is part of Cyclone.
 *
 * Cyclone is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 */


// Settings ranges and dialog conversions for Cyclone.
//
// This header deliberately has no windows.h dependency. Registry values,
// dialog controls and defaults all share these bounds, and the conversions can
// be tested without opening a real settings dialog or touching HKCU.

#ifndef CYCLONE_SETTINGS_H
#define CYCLONE_SETTINGS_H

namespace cycloneSettings {

struct Range { int lo; int hi; };

const Range kCyclones   = { 1, 10 };
const Range kParticles  = { 1, 10000 };
const Range kSize       = { 1, 100 };
const Range kComplexity = { 1, 10 };
const Range kSpeed      = { 1, 100 };
const Range kFrameRate  = { 1, 1000 };  // Stored 0 means unlimited.

const int kDefaultCyclones   = 1;
const int kDefaultParticles  = 400;
const int kDefaultSize       = 7;
const int kDefaultComplexity = 3;
const int kDefaultSpeed      = 10;
const int kDefaultFrameRate  = 60;  // Offered while limiting is switched off.


// Registry values arrive as DWORD. Keeping this unsigned until after the upper
// bound check prevents 0xFFFFFFFF from narrowing to -1 and bypassing the clamp.
inline int clampToRange(unsigned long value, Range range){
	if(value > (unsigned long)range.hi) return range.hi;
	if((int)value < range.lo) return range.lo;
	return (int)value;
}


inline int normalizeFlag(unsigned long value){
	return value != 0 ? 1 : 0;
}


inline unsigned int clampFrameRate(unsigned long stored){
	if(stored == 0) return 0;
	return (unsigned int)clampToRange(stored, kFrameRate);
}


struct FrameRateUi { bool limited; int fps; };

inline FrameRateUi frameRateToUi(unsigned int stored){
	const unsigned int clamped = clampFrameRate(stored);
	FrameRateUi ui;
	ui.limited = (clamped != 0);
	ui.fps = ui.limited ? (int)clamped : kDefaultFrameRate;
	return ui;
}


inline unsigned int frameRateFromUi(bool limited, int fps){
	if(!limited) return 0;
	return (unsigned int)clampToRange(fps < 0 ? 0UL : (unsigned long)fps,
		kFrameRate);
}

}

#endif
