/*
 * Copyright (C) 1999-2010  Terence M. Welsh
 *
 * This file is part of Skyrocket.
 *
 * Skyrocket is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Skyrocket is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// Settings ranges for Skyrocket.
//
// Deliberately free of windows.h so it can be unit tested on any platform.

#ifndef SKYROCKET_SETTINGS_H
#define SKYROCKET_SETTINGS_H

#include "../common/saverSettings.h"

namespace skyrocketSettings {

// Re-exported, never redefined. The bodies live once in rssaver; without these
// three lines a qualified call such as skyrocketSettings::clampToRange(...)
// does not compile, because argument-dependent lookup does not apply to
// qualified names and there is nothing of that name in this namespace.
using rssaver::Range;
using rssaver::clampToRange;
using rssaver::clampIntToRange;

// 13 settings, matching the 13 saver-specific reads in skyrocket.cpp's
// readRegistry. dFrameRateLimit is rsWin32Saver's and is clamped by
// rsWin32Saver::clampFrameRateLimit.
constexpr Range kMaxrockets      = { 1, 100 };  // TBM_SETRANGE
constexpr Range kSmoke           = { 0, 60 };   // TBM_SETRANGE
// Tracks WHICHSMOKES; see the static_assert in skyrocket.cpp.
constexpr Range kExplosionsmoke  = { 0, 100 };  // TBM_SETRANGE
constexpr Range kWind            = { 0, 100 };  // TBM_SETRANGE
constexpr Range kAmbient         = { 0, 100 };  // TBM_SETRANGE
constexpr Range kStardensity     = { 0, 100 };  // TBM_SETRANGE
constexpr Range kFlare           = { 0, 100 };  // TBM_SETRANGE
constexpr Range kMoonglow        = { 0, 100 };  // TBM_SETRANGE
constexpr Range kSound           = { 0, 100 };  // TBM_SETRANGE
constexpr Range kMoon            = { 0, 1 };    // CheckDlgButton
constexpr Range kClouds          = { 0, 1 };    // CheckDlgButton
constexpr Range kEarth           = { 0, 1 };    // CheckDlgButton
constexpr Range kIllumination    = { 0, 1 };    // CheckDlgButton

}  // namespace skyrocketSettings

#endif
