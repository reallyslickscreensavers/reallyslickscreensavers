/*
 * Copyright (C) 2010  Terence M. Welsh
 *
 * This file is part of Microcosm.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// Settings ranges for Microcosm.
//
// Deliberately free of windows.h so it can be unit tested on any platform.

#ifndef MICROCOSM_SETTINGS_H
#define MICROCOSM_SETTINGS_H

#include "../common/saverSettings.h"

namespace microcosmSettings {

// Re-exported, never redefined. The bodies live once in rssaver; without these
// three lines a qualified call such as microcosmSettings::clampToRange(...)
// does not compile, because argument-dependent lookup does not apply to
// qualified names and there is nothing of that name in this namespace.
using rssaver::Range;
using rssaver::clampToRange;
using rssaver::clampIntToRange;

// 11 settings, matching the 11 saver-specific reads in microcosm.cpp's
// readRegistry. dFrameRateLimit is rsWin32Saver's and is clamped by
// rsWin32Saver::clampFrameRateLimit.
constexpr Range kKaleidoscopeTime = { 0, 300 };  // TBM_SETRANGE
constexpr Range kSingleTime       = { 0, 300 };  // TBM_SETRANGE
// The registry key is "SingleBackground" (microcosm.cpp) while the global is
// dBackground; the constant follows the global, so kBackground - never a
// constant name built from the registry key spelling.
constexpr Range kBackground       = { 0, 100 };  // TBM_SETRANGE
constexpr Range kResolution       = { 20, 100 }; // TBM_SETRANGE
constexpr Range kDepth            = { 1, 10 };   // TBM_SETRANGE
constexpr Range kFov              = { 10, 150 }; // TBM_SETRANGE
constexpr Range kGizmoSpeed       = { 1, 100 };  // TBM_SETRANGE
constexpr Range kColorSpeed       = { 0, 100 };  // TBM_SETRANGE
constexpr Range kCameraSpeed      = { 1, 100 };  // TBM_SETRANGE
constexpr Range kShaders          = { 0, 1 };    // CheckDlgButton
constexpr Range kFog              = { 0, 1 };    // CheckDlgButton

}  // namespace microcosmSettings

#endif
