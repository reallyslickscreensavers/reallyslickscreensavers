/*
 * Copyright (C) 2000-2010  Terence M. Welsh
 *
 * This file is part of Euphoria.
 *
 * Euphoria is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Euphoria is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// Settings ranges for Euphoria.
//
// Deliberately free of windows.h so it can be unit tested on any platform.

#ifndef EUPHORIA_SETTINGS_H
#define EUPHORIA_SETTINGS_H

#include "../common/saverSettings.h"

namespace euphoriaSettings {

// Re-exported, never redefined. The bodies live once in rssaver; without these
// three lines a qualified call such as euphoriaSettings::clampToRange(...)
// does not compile, because argument-dependent lookup does not apply to
// qualified names and there is nothing of that name in this namespace.
using rssaver::Range;
using rssaver::clampToRange;
using rssaver::clampIntToRange;

// 10 settings, matching the 10 saver-specific reads in euphoria.cpp's
// readRegistry. dFrameRateLimit is rsWin32Saver's and is clamped by
// rsWin32Saver::clampFrameRateLimit.
constexpr Range kWisps         = { 0, 100 };  // UDM_SETRANGE
// A count of background wisps (euphoria.cpp: `new wisp[dBackground]`).
constexpr Range kBackground    = { 0, 100 };  // UDM_SETRANGE
constexpr Range kDensity       = { 2, 100 };  // TBM_SETRANGE
constexpr Range kVisibility    = { 1, 100 };  // TBM_SETRANGE
constexpr Range kSpeed         = { 1, 100 };  // TBM_SETRANGE
constexpr Range kFeedback      = { 0, 100 };  // TBM_SETRANGE
constexpr Range kFeedbackspeed = { 1, 100 };  // TBM_SETRANGE
// Tracks the FEEDBACKSIZE_MIN/MAX macros; see the static_assert in euphoria.cpp.
constexpr Range kFeedbacksize  = { 1, 10 };   // TBM_SETRANGE
constexpr Range kTexture       = { 0, 4 };    // 5 CB_ADDSTRING entries
constexpr Range kWireframe     = { 0, 1 };    // CheckDlgButton

}  // namespace euphoriaSettings

#endif
