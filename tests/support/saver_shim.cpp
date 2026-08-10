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

#include <windows.h>

#include <rsWin32Saver/rsWin32Saver.h>

HINSTANCE mainInstance = 0;
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
}

void initFrameRateLimitSlider(HWND, int, int)
{
}

void updateFrameRateLimitSlider(HWND, int, int)
{
}

void readFrameRateLimitFromRegistry()
{
}

void writeFrameRateLimitToRegistry()
{
}
