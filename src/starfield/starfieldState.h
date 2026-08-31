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


// Starfield's module state, in one struct reached through a function-local
// static (Task 6 in docs/MAINTENANCE.md, SonarCloud cpp:S5421).
//
// The rationale lives in docs/MAINTENANCE.md rather than in thirteen copies of
// this comment. What has to be repeated here is only what a reader of this file
// needs:
//
//  - Hoist "auto& s = state();" once per function, never inside a loop, so the
//    thread-safe-static guard is not paid per iteration.
//  - It is "auto&", never "auto". The copy constructor is deleted so that the
//    dropped ampersand is a compile error rather than a silent copy whose
//    writes go nowhere.
//  - registryPath is not here. rsWin32Saver.h declares it extern and the
//    framework reads it, so it stays a global.
//  - Nothing resets this struct. cleanUp keeps exactly the resets it performed
//    as separate globals; a wholesale reset would also zero the settings that
//    readRegistry filled once at WM_CREATE.

#ifndef STARFIELD_STATE_H
#define STARFIELD_STATE_H

#include <memory>
#include <vector>

#ifdef WIN32
#include <windows.h>
#endif

#include <rsText/rsText.h>

namespace starfieldState {

struct State {
	State() = default;
	State(const State&) = delete;
	State& operator=(const State&) = delete;

#ifdef WIN32
	HGLRC hglrc = nullptr;
	HDC hdc = nullptr;
#endif
	int readyToDraw = 0;
	float frameTime = 0.0f;
	float aspectRatio = 0.0f;

	// Star data
	std::vector<float> starX;
	std::vector<float> starY;
	std::vector<float> starZ;
	std::vector<float> starV;  // per-star velocity multiplier

	// text output
	std::unique_ptr<rsText> textwriter;

	// Parameters edited in the dialog box
	int dNumStars = 0;
	int dSpeed = 0;
	int dStarSize = 0;
};

// Defined in starfield.cpp. External linkage on purpose: the test suite reaches
// the saver's state through this rather than declaring externs of its own.
State& state();

}  // namespace starfieldState

#endif
