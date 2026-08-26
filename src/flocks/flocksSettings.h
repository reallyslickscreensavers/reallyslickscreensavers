/*
 * Copyright (C) 1999-2010  Terence M. Welsh
 *
 * This file is part of Flocks.
 *
 * Flocks is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Flocks is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// Settings ranges for Flocks.
//
// Deliberately free of windows.h so it can be unit tested on any platform.

#ifndef FLOCKS_SETTINGS_H
#define FLOCKS_SETTINGS_H

#include "../common/saverSettings.h"

namespace flocksSettings {

// Re-exported, never redefined. The bodies live once in rssaver; without these
// three lines a qualified call such as flocksSettings::clampToRange(...) does
// not compile, because argument-dependent lookup does not apply to qualified
// names and there is nothing of that name in this namespace.
using rssaver::Range;
using rssaver::clampToRange;
using rssaver::clampIntToRange;

// 10 settings, matching the 10 saver-specific reads in flocks.cpp's
// readRegistry. dFrameRateLimit is rsWin32Saver's and is clamped by
// rsWin32Saver::clampFrameRateLimit.
constexpr Range kLeaders         = { 1, 100 };    // UDM_SETRANGE
constexpr Range kFollowers       = { 0, 10000 };  // UDM_SETRANGE
constexpr Range kGeometry        = { 0, 1 };      // 2 CB_ADDSTRING entries (Dots/Blobs)
constexpr Range kSize            = { 1, 100 };    // TBM_SETRANGE
constexpr Range kComplexity      = { 1, 10 };     // TBM_SETRANGE
constexpr Range kSpeed           = { 1, 100 };    // TBM_SETRANGE
// A slider here, unlike cyclone's checkbox of the same name.
constexpr Range kStretch         = { 0, 100 };    // TBM_SETRANGE
constexpr Range kColorfadespeed  = { 0, 100 };    // TBM_SETRANGE
constexpr Range kChromatek       = { 0, 1 };      // CheckDlgButton
constexpr Range kConnections     = { 0, 1 };      // CheckDlgButton

}  // namespace flocksSettings

#endif
