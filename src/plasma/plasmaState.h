/*
 * Copyright (C) 1999-2010  Terence M. Welsh
 *
 * This file is part of Plasma.
 *
 * Plasma is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Plasma is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// Plasma's module state, in one struct reached through a function-local static
// (Task 6 in docs/MAINTENANCE.md, SonarCloud cpp:S5421). The rationale lives in
// that document; src/starfield/starfieldState.h is the worked example.
//
// Plasma is the reason the copy constructor is deleted rather than merely
// discouraged: this struct is about 32 MB, so "auto s = state()" instead of
// "auto& s = state()" would copy that on every call while quietly dropping
// every write. Deleting the copy makes it a compile error.

#ifndef PLASMA_STATE_H
#define PLASMA_STATE_H

#ifdef WIN32
#include <windows.h>
#endif

#include <rsText/rsText.h>

namespace plasmaState {

constexpr int kNumConsts = 18;
constexpr int kTexSize = 1024;

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
	float wide = 0.0f;
	float high = 0.0f;
	float c[kNumConsts] = {};   // constant
	float ct[kNumConsts] = {};  // temporary value of constant
	float cv[kNumConsts] = {};  // velocity of constant

	// The three big arrays carry no initialiser on purpose. A static object is
	// zero-initialised before any constructor runs, which is exactly what these
	// were as globals; writing "= {}" here would instead emit a 32 MB store on
	// the first call to state().
	float position[kTexSize][kTexSize][2];
	float plasma[kTexSize][kTexSize][3];
	float plasmamap[kTexSize * kTexSize * 3];

	unsigned int tex = 0;
	int plasmasize = 64;

	// text output. Raw, and never deleted, exactly as it was as a global -
	// migrations are a pure move. Giving plasma a teardown for this is a
	// separate change with its own test.
	rsText* textwriter = nullptr;

	// Parameters edited in the dialog box
	int dZoom = 0;
	int dFocus = 0;
	int dSpeed = 0;
	int dResolution = 0;
};

// Defined in plasma.cpp. External linkage on purpose: the test suite reaches
// the saver's state through this rather than declaring externs of its own.
State& state();

}  // namespace plasmaState

#endif
