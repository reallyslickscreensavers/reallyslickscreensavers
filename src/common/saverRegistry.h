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


// Typed registry reads for every saver's readRegistry().
//
// This is the windows.h half of the settings work, kept out of
// common/saverSettings.h on purpose: that header is free of windows.h so the
// ranges and clamps can be unit tested without an HWND or a registry, and
// tests/test_saverSettings.cpp compiling at all is what asserts it stayed that
// way. Anything needing HKEY or RegQueryValueEx belongs here instead.
//
// Every saver used to read settings as:
//
//     result = RegQueryValueEx(skey, "Speed", 0, &valtype, (LPBYTE)&val, &valsize);
//     if(result == ERROR_SUCCESS)
//         dSpeed = ...;
//
// which accepts a value of any type: a "Speed" stored as REG_SZ was handed
// straight to the saver as the first four bytes of its text, reinterpreted as
// a DWORD. readRegistryDWORD checks the type and the size before reporting
// success, so a wrongly typed value now leaves the default in place instead.

#ifndef RSSAVER_COMMON_SAVERREGISTRY_H
#define RSSAVER_COMMON_SAVERREGISTRY_H

#include <windows.h>

namespace rssaver {

// True only when 'name' exists under 'key' AND is a DWORD of the expected
// size. On false, 'value' is not meaningfully set and the caller must leave
// its setting alone - which is what keeps setDefaults' value intact.
inline bool readRegistryDWORD(HKEY key, LPCTSTR name, DWORD& value) {
    DWORD type = 0;
    DWORD size = sizeof(value);
    return RegQueryValueEx(key, name, nullptr, &type, (LPBYTE)&value, &size) == ERROR_SUCCESS
        && type == REG_DWORD
        && size == sizeof(value);
}

}  // namespace rssaver

#endif
