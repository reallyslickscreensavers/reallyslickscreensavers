/*
 * Copyright (C) 1999-2010  Terence M. Welsh
 *
 * This file is part of Lattice.
 *
 * Lattice is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Lattice is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// Settings ranges for Lattice.
//
// Deliberately free of windows.h so it can be unit tested on any platform.

#ifndef LATTICE_SETTINGS_H
#define LATTICE_SETTINGS_H

#include "../common/saverSettings.h"

namespace latticeSettings {

// Re-exported, never redefined. The bodies live once in rssaver; without these
// three lines a qualified call such as latticeSettings::clampToRange(...) does
// not compile, because argument-dependent lookup does not apply to qualified
// names and there is nothing of that name in this namespace.
using rssaver::Range;
using rssaver::clampToRange;
using rssaver::clampIntToRange;

// 11 settings, matching the 11 saver-specific reads in lattice.cpp's
// readRegistry. dFrameRateLimit is rsWin32Saver's and is clamped by
// rsWin32Saver::clampFrameRateLimit.
constexpr Range kLongitude  = { 4, 100 };   // UDM_SETRANGE
constexpr Range kLatitude   = { 2, 100 };   // UDM_SETRANGE
constexpr Range kThick      = { 1, 100 };   // TBM_SETRANGE
constexpr Range kDensity    = { 1, 100 };   // TBM_SETRANGE
// Tracks LATSIZE - 2; see the static_assert in lattice.cpp.
constexpr Range kDepth      = { 1, 10 };    // TBM_SETRANGE
constexpr Range kFov        = { 10, 150 };  // TBM_SETRANGE
constexpr Range kPathrand   = { 1, 10 };    // TBM_SETRANGE
constexpr Range kSpeed      = { 1, 100 };   // TBM_SETRANGE
constexpr Range kTexture    = { 0, 9 };     // 10 CB_ADDSTRING entries, last is "random"
constexpr Range kSmooth     = { 0, 1 };     // CheckDlgButton
constexpr Range kFog        = { 0, 1 };     // CheckDlgButton

}  // namespace latticeSettings

#endif
