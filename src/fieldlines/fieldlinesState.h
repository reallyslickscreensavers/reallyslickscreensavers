/*
 * Copyright (C) 1999-2010  Terence M. Welsh
 *
 * This file is part of Field Lines.
 *
 * Field Lines is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Field Lines is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// Field Lines' module state, in one struct reached through a function-local
// static (Task 6 in docs/MAINTENANCE.md, SonarCloud cpp:S5421). The rationale
// lives in that document; src/starfield/starfieldState.h is the worked example.

#ifndef FIELDLINES_STATE_H
#define FIELDLINES_STATE_H

#include <Windows.h>

#include <rsText/rsText.h>

// Defined in fieldlines.cpp, below the state it is reached through. The struct
// only needs to hold a pointer to it.
class ion;

namespace fieldlinesState {

struct State {
	State() = default;
	State(const State&) = delete;
	State& operator=(const State&) = delete;

	HDC hdc = nullptr;
	HGLRC hglrc = nullptr;
	int readyToDraw = 0;

	// Half-extents of the box the ions move inside, set from the window shape.
	float wide = 0.0f;
	float high = 0.0f;
	float deep = 0.0f;

	// Allocated in initSaver, deleted in cleanUp. Ownership is unchanged by the
	// move into this struct.
	ion* ions = nullptr;

	float aspectRatio = 0.0f;
	float frameTime = 0.0f;

	// text output
	rsText* textwriter = nullptr;

	// Parameters edited in the dialog box
	int dIons = 0;
	int dStepSize = 0;
	int dMaxSteps = 0;
	int dWidth = 0;
	int dSpeed = 0;
	BOOL dConstwidth = FALSE;
	BOOL dElectric = FALSE;
};

// Defined in fieldlines.cpp. External linkage on purpose: the test suite reaches
// the saver's state through this rather than declaring externs of its own.
State& state();

}  // namespace fieldlinesState

#endif
