#include "test_window.h"

namespace testsupport {
namespace {

LRESULT CALLBACK testWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	return DefWindowProc(hwnd, msg, wp, lp);
}

HWND create()
{
	static const char* kClassName = "RsSaverTestHost";

	WNDCLASSA wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.lpfnWndProc = testWndProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = kClassName;
	RegisterClassA(&wc);   // harmless if already registered

	// WS_POPUP with an explicit client size, never shown. AdjustWindowRect is
	// not needed for WS_POPUP without a frame: the client area is the whole
	// window, so GetClientRect returns exactly kHostWidth x kHostHeight.
	return CreateWindowExA(
		0, kClassName, "rssavers test host", WS_POPUP,
		0, 0, kHostWidth, kHostHeight,
		nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
}

}  // namespace

HWND hostWindow()
{
	// Function-local so the handle is not a mutable global; created on first
	// use and reused for the life of the process.
	static HWND window = create();

	// Falling back to the desktop keeps the suite runnable if a runner ever
	// denies window creation, at the cost of the determinism above - so it is
	// worth noticing rather than silently accepting.
	if (!window) {
		return GetDesktopWindow();
	}
	return window;
}

}  // namespace testsupport
