/*
 * Copyright (C) 1999-2010  Terence M. Welsh
 *
 * This file is part of Really Slick Screensavers.
 *
 * Really Slick Screensavers is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Really Slick Screensavers is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// Shared settings-clamping primitives for every saver's <name>Settings.h.
//
// Deliberately free of windows.h so it can be unit tested on any platform.
// Each saver's own settings header re-exports Range, clampToRange and
// clampIntToRange with `using` declarations rather than redefining them, so
// the bodies live exactly once.

#ifndef RSSAVER_COMMON_SAVERSETTINGS_H
#define RSSAVER_COMMON_SAVERSETTINGS_H

namespace rssaver {

struct Range { int lo; int hi; };

// Registry values arrive as DWORD. Narrowing to int first turns 0xFFFFFFFF
// into -1, which slips past a naive lower-bound check, so the parameter is
// unsigned long and the upper bound is tested first, unsigned.
constexpr int clampToRange(unsigned long v, Range r) {
    if (v > (unsigned long)r.hi) return r.hi;
    if ((int)v < r.lo) return r.lo;
    return (int)v;
}

// For a value already held in a signed int, where -5 must clamp to lo and not
// to hi. Deliberately a different name, not an overload: unsigned int would be
// an ambiguous conversion to both int and unsigned long.
constexpr int clampIntToRange(int v, Range r) {
    if (v < r.lo) return r.lo;
    if (v > r.hi) return r.hi;
    return v;
}

}  // namespace rssaver

#endif
