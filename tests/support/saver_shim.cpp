/*
 * Win32 side of the saver test harness.
 *
 * Defines the globals rsWin32Saver normally owns (rsWin32Saver.h:57-77). We do
 * not link rsWin32Saverd.lib because it carries WinMain, which would fight
 * gtest_main's main.
 *
 * registryPath is NOT defined here: each saver defines its own, and that
 * definition arrives with the saver's .cpp.
 */

#include <Windows.h>

#include <rsWin32Saver/rsWin32Saver.h>

// SonarCloud cpp:S5421 flags every one of these as a mutable global, and it is
// right that they are. They cannot be anything else: rsWin32Saver.h declares
// them extern and the savers both read and write them, so this file has to
// *define* them at namespace scope with exactly these names and types or
// nothing links. Making them const would not compile against the savers.
//
// The finding therefore belongs to the framework's design - Task 6 in
// docs/MAINTENANCE.md, "encapsulate mutable module globals" - and not to the
// test harness, which only supplies what rsWin32Saverd.lib normally would.
HINSTANCE mainInstance = nullptr;
HWND mainWindow = 0;
int isSuspended = 0;
int doingPreview = 0;
int pfd_swap_exchange = 0;
int pfd_swap_copy = 0;
unsigned int dFrameRateLimit = 0;
int kStatistics = 0;

// Also owned by rsWin32Saver in a real build, and referenced by some savers.
int childPreview = 0;
int windowedSaver = 0;
int reallyClose = 0;


// ---------------------------------------------------------------------------
// Test doubles for the framework functions a saver calls.
//
// These are the other half of not linking rsWin32Saverd.lib. Each one only
// drives Win32 windows or dialog controls, so a no-op is the honest stand-in:
// there is no window and no dialog in a test process. The logic that used to
// hide inside them now lives in rsWin32SaverSettings.h and is tested in rslibs.
// ---------------------------------------------------------------------------

LRESULT defScreenSaverProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void setBestPixelFormat(HDC)
{
	// Intentionally empty: chooses and sets a PIXELFORMATDESCRIPTOR on a real
	// device context. There is no drawing surface in a test process, and the GL
	// stub does not care what format was negotiated.
}

void initFrameRateLimitSlider(HWND, int, int)
{
	// Intentionally empty: only sends TBM_SETRANGE/TBM_SETPOS to a trackbar
	// that does not exist here. The decidable part - the 0..1000 bounds and the
	// clamp - lives in rsWin32SaverSettings.h and is tested in rslibs.
}

void updateFrameRateLimitSlider(HWND, int, int)
{
	// Intentionally empty, for the same reason as initFrameRateLimitSlider.
}

void readFrameRateLimitFromRegistry()
{
	// Intentionally empty: the real one reads HKCU and is covered by rslibs.
	// Doing nothing leaves dFrameRateLimit at its default, which is what a
	// machine with no stored settings would produce anyway.
}

void writeFrameRateLimitToRegistry()
{
	// Intentionally empty, and deliberately so: the real one writes to HKCU.
	// A test process must never modify the developer's saver settings.
}
