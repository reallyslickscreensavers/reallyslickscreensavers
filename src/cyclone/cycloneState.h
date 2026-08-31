/*
 * Copyright (C) 1999-2010  Terence M. Welsh
 *
 * This file is part of Cyclone.
 *
 * Cyclone is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Cyclone is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// Cyclone's module state, in one struct reached through a function-local static
// (Task 6 in docs/MAINTENANCE.md, SonarCloud cpp:S5421). The rationale lives in
// that document; src/starfield/starfieldState.h is the worked example.
//
// Include this before cyclone.cpp's "#define wide 200" / "#define high 200".
// Those are unqualified object-like macros, so any header pulled in after them
// that happens to use either word is rewritten.

#ifndef CYCLONE_STATE_H
#define CYCLONE_STATE_H

#include <windows.h>

#include <rsText/rsText.h>

// Both defined in cyclone.cpp, below the state they are reached through. The
// struct only needs to hold pointers to them.
class cyclone;
class particle;

namespace cycloneState {

// factorial() fills this once at startup and the curve maths indexes it by
// dComplexity, whose declared range tops out well inside it.
constexpr int kFactorialTableSize = 13;

struct State {
	State() = default;
	State(const State&) = delete;
	State& operator=(const State&) = delete;

	HGLRC hglrc = nullptr;
	HDC hdc = nullptr;
	int readyToDraw = 0;

	// Parameters edited in the dialog box
	int dCyclones = 0;
	int dParticles = 0;
	int dSize = 0;
	int dComplexity = 0;
	int dSpeed = 0;
	BOOL dStretch = FALSE;
	BOOL dShowCurves = FALSE;

	// Other state
	float aspectRatio = 0.0f;

	// Both allocated in initSaver and deleted in cleanUp. Ownership is
	// unchanged by the move into this struct.
	cyclone** cyclones = nullptr;
	particle** particles = nullptr;

	float fact[kFactorialTableSize] = {};
	float frameTime = 0.0f;

	// text output
	rsText* textwriter = nullptr;
};

// Defined in cyclone.cpp. External linkage on purpose: the test suite reaches
// the saver's state through this rather than declaring externs of its own.
State& state();

}  // namespace cycloneState

#endif
