/*
 * Copyright (C) 2005-2010  Terence M. Welsh
 *
 * This file is part of Hyperspace.
 *
 * Hyperspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Hyperspace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// Settings ranges for Hyperspace.
//
// Deliberately free of windows.h so it can be unit tested on any platform.

#ifndef HYPERSPACE_SETTINGS_H
#define HYPERSPACE_SETTINGS_H

#include "../common/saverSettings.h"

namespace hyperspaceSettings {

// Re-exported, never redefined. The bodies live once in rssaver; without these
// three lines a qualified call such as hyperspaceSettings::clampToRange(...)
// does not compile, because argument-dependent lookup does not apply to
// qualified names and there is nothing of that name in this namespace.
using rssaver::Range;
using rssaver::clampToRange;
using rssaver::clampIntToRange;

// 9 settings, matching the 9 saver-specific reads in hyperspace.cpp's
// readRegistry. dFrameRateLimit is rsWin32Saver's and is clamped by
// rsWin32Saver::clampFrameRateLimit.
constexpr Range kSpeed        = { 1, 100 };    // TBM_SETRANGE
constexpr Range kStars        = { 0, 10000 };  // TBM_SETRANGE
constexpr Range kStarSize     = { 1, 100 };    // TBM_SETRANGE
constexpr Range kResolution   = { 4, 20 };     // TBM_SETRANGE
constexpr Range kDepth        = { 1, 10 };     // TBM_SETRANGE
constexpr Range kFov          = { 10, 150 };   // TBM_SETRANGE
constexpr Range kUseTunnels   = { 0, 1 };      // CheckDlgButton
constexpr Range kUseGoo       = { 0, 1 };      // CheckDlgButton
constexpr Range kShaders      = { 0, 1 };      // CheckDlgButton

}  // namespace hyperspaceSettings

#endif
