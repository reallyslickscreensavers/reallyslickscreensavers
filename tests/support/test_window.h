/*
 * A deterministic host window for saver tests.
 *
 * Every saver's initSaver does GetClientRect(hwnd) and derives aspectRatio -
 * and through it the whole simulation grid - from the result. Handing it
 * GetDesktopWindow() makes the tests inherit whatever screen the machine
 * happens to have, and a headless CI runner reports something quite unlike a
 * developer's desktop.
 *
 * That does not fail loudly. It silently changes how much of each saver runs:
 * plasma sizes its field from aspectRatio, so a degenerate rect leaves
 * plasmasize at 0 and the field loops never execute. Coverage on the runner
 * came out about five points below local for every module before this existed.
 *
 * So tests use a real, hidden, fixed-size window instead.
 */

#ifndef TEST_WINDOW_H
#define TEST_WINDOW_H

#include <Windows.h>

namespace testsupport {

// 4:3, chosen so aspectRatio is a clean 1.333 and the landscape branch runs.
const int kHostWidth = 640;
const int kHostHeight = 480;

// Created once on first use and reused. Never shown.
HWND hostWindow();

}  // namespace testsupport

#endif
