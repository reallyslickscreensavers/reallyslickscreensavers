/*
 * Copyright (C) 1999-2010  Terence M. Welsh
 *
 * This file is part of Starfield.
 *
 * Starfield is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Starfield is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// Starfield screen saver

#ifdef WIN32
#include <windows.h>
#include <rsWin32Saver/rsWin32Saver.h>
#include <rsWin32Saver/rsWin32SaverSettings.h>
#include <process.h>
#include <regstr.h>
#include <commctrl.h>
#include "resource.h"
#endif
#ifdef RS_XSCREENSAVER
#include <rsXScreenSaver/rsXScreenSaver.h>
#endif

#include <stdio.h>
#include <math.h>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <rsText/rsText.h>
#include <rsMath/rsMath.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "starfieldSettings.h"
#include "starfieldState.h"
#include "../common/saverRegistry.h"


// Globals
#ifdef WIN32
// The one global the framework mandates: rsWin32Saver.h declares it extern and
// reads it back. Everything else this module owns is in starfieldState::State.
LPCTSTR registryPath = TEXT("Software\\Really Slick\\Starfield");
#endif

starfieldState::State& starfieldState::state(){
	static State s;
	return s;
}

using starfieldState::state;

// Constants
const float farZ = 200.0f;
const float nearZ = 0.1f;
const float fovHalfTan = 0.41421356f;  // tan(22.5 degrees) for 45 degree vertical FOV
const int maxStarSize = 10;  // matches STARSIZE slider and -starsize CLI range


void initStar(int i){
	auto& s = state();
	s.starZ[i] = rsRandf(farZ - nearZ) + nearZ;
	float halfH = fovHalfTan * s.starZ[i];  // visible half-height at this depth
	float halfW = halfH * s.aspectRatio;    // visible half-width at this depth
	s.starX[i] = rsRandf(halfW * 2.0f) - halfW;
	s.starY[i] = rsRandf(halfH * 2.0f) - halfH;
	s.starV[i] = rsRandf(3.0f) + 0.15f;  // velocity multiplier 0.15 to 3.15
}


void draw(){
	auto& s = state();

	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	float baseSpeed = float(s.dSpeed) * 0.5f;  // scaled speed

	// Stars are grouped by rounded point size so each size needs only one
	// glBegin/glEnd pair per frame instead of one per star.
	static std::array<std::vector<int>, maxStarSize + 1> sizeBuckets;  // index 0 unused
	static std::vector<float> starBrightness;
	if(starBrightness.size() != size_t(s.dNumStars))
		starBrightness.resize(s.dNumStars);
	for(auto& bucket : sizeBuckets)
		bucket.clear();

	// Update stars
	for(int i = 0; i < s.dNumStars; i++){
		// Move star toward viewer
		s.starZ[i] -= baseSpeed * s.starV[i] * s.frameTime;

		// Respawn if past viewer or outside visible frustum
		float halfH = fovHalfTan * s.starZ[i];
		float halfW = halfH * s.aspectRatio;
		if(s.starZ[i] < nearZ || s.starX[i] < -halfW || s.starX[i] > halfW
			|| s.starY[i] < -halfH || s.starY[i] > halfH){
			s.starZ[i] = farZ - rsRandf(10.0f);  // respawn near far plane
			halfH = fovHalfTan * s.starZ[i];
			halfW = halfH * s.aspectRatio;
			s.starX[i] = rsRandf(halfW * 2.0f) - halfW;
			s.starY[i] = rsRandf(halfH * 2.0f) - halfH;
			s.starV[i] = rsRandf(3.0f) + 0.15f;
		}

		// Brightness and size based on distance (closer = brighter and bigger)
		float brightness = 1.0f - (s.starZ[i] / farZ);
		if(brightness < 0.0f) brightness = 0.0f;
		if(brightness > 1.0f) brightness = 1.0f;
		starBrightness[i] = brightness;

		// Bucket by rounded size. NaN fails every comparison, so the range
		// test is written as "is in range" rather than "is out of range":
		// anything that is not - NaN included - falls to the minimum instead
		// of slipping past two ifs and indexing sizeBuckets far out of
		// bounds. Task 12 in docs/MAINTENANCE.md has how that was reachable.
		const float size = float(s.dStarSize) * brightness;
		int bucket = 1;
		if(size >= 1.0f)
			bucket = (size <= float(maxStarSize)) ? int(size + 0.5f) : maxStarSize;
		sizeBuckets[bucket].push_back(i);
	}

	// Render stars, one batch per point size
	for(int bucketSize = 1; bucketSize <= maxStarSize; bucketSize++){
		if(sizeBuckets[bucketSize].empty()) continue;
		glPointSize(float(bucketSize));
		glBegin(GL_POINTS);
		for(int idx : sizeBuckets[bucketSize]){
			float b = starBrightness[idx];
			glColor3f(b, b, b);
			glVertex3f(s.starX[idx], s.starY[idx], -s.starZ[idx]);
		}
		glEnd();
	}

	// print text
	static float totalTime = 0.0f;
	totalTime += s.frameTime;
	static std::string str;
	static int frames = 0;
	++frames;
	if(frames == 20){
		str = "FPS = " + std::to_string(20.0f / totalTime);
		totalTime = 0.0f;
		frames = 0;
	}
	if(kStatistics){
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrtho(0.0f, 50.0f * s.aspectRatio, 0.0f, 50.0f, -1.0f, 1.0f);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glTranslatef(1.0f, 48.0f, 0.0f);

		glColor3f(1.0f, 0.6f, 0.0f);
		s.textwriter->draw(str);

		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
	}

#ifdef WIN32
	wglSwapLayerBuffers(s.hdc, WGL_SWAP_MAIN_PLANE);
#endif
#ifdef RS_XSCREENSAVER
	glXSwapBuffers(xdisplay, xwindow);
#endif
}


void idleProc(){
	auto& s = state();

	// update time
	static rsTimer timer;
	s.frameTime = float(timer.tick());

#ifdef RS_XSCREENSAVER
	const bool shouldDraw = (s.readyToDraw && !isSuspended && !checkingPassword);
#else
	const bool shouldDraw = (s.readyToDraw && !isSuspended);
#endif
	if(shouldDraw)
		draw();
}


void setDefaults(){
	auto& s = state();
	s.dNumStars = starfieldSettings::kDefaultNumStars;
	s.dSpeed = starfieldSettings::kDefaultSpeed;
	s.dStarSize = starfieldSettings::kDefaultStarSize;
	dFrameRateLimit = 0;  // unlimited
}


void allocateStars(){
	auto& s = state();
	s.starX.resize(s.dNumStars);
	s.starY.resize(s.dNumStars);
	s.starZ.resize(s.dNumStars);
	s.starV.resize(s.dNumStars);
	for(int i = 0; i < s.dNumStars; i++)
		initStar(i);
}


#ifdef RS_XSCREENSAVER
void handleCommandLine(int argc, char* argv[]){
	auto& s = state();
	setDefaults();
	getArgumentsValue(argc, argv, std::string("-numstars"), s.dNumStars,
		starfieldSettings::kNumStars.lo, starfieldSettings::kNumStars.hi);
	getArgumentsValue(argc, argv, std::string("-speed"), s.dSpeed,
		starfieldSettings::kSpeed.lo, starfieldSettings::kSpeed.hi);
	getArgumentsValue(argc, argv, std::string("-starsize"), s.dStarSize,
		starfieldSettings::kStarSize.lo, starfieldSettings::kStarSize.hi);
}

void reshape(int width, int height){
	auto& s = state();
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	s.aspectRatio = float(width) / float(height);
	gluPerspective(45.0, double(s.aspectRatio), double(nearZ), double(farZ));
	glMatrixMode(GL_MODELVIEW);
}
#endif


#ifdef WIN32
void initSaver(HWND hwnd){
	auto& s = state();
	RECT rect;

	// Window initialization
	s.hdc = GetDC(hwnd);
	setBestPixelFormat(s.hdc);
	s.hglrc = wglCreateContext(s.hdc);
	GetClientRect(hwnd, &rect);
	wglMakeCurrent(s.hdc, s.hglrc);
	glViewport(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
	s.aspectRatio = float(rect.right - rect.left) / float(rect.bottom - rect.top);
#endif
#ifdef RS_XSCREENSAVER
void initSaver(){
	auto& s = state();
#endif

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_POINT_SMOOTH);
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);

	// Perspective projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0, double(s.aspectRatio), double(nearZ), double(farZ));

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	allocateStars();

	// Initialize text
	s.textwriter = std::make_unique<rsText>();

	s.readyToDraw = 1;
}


void freeStars(){
	auto& s = state();
	s.starX.clear(); s.starX.shrink_to_fit();
	s.starY.clear(); s.starY.shrink_to_fit();
	s.starZ.clear(); s.starZ.shrink_to_fit();
	s.starV.clear(); s.starV.shrink_to_fit();
}


#ifdef RS_XSCREENSAVER
void cleanUp(){
	freeStars();
	state().textwriter.reset();
}
#endif


#ifdef WIN32
void cleanUp(HWND hwnd){
	auto& s = state();
	freeStars();
	s.textwriter.reset();
	// Kill device context
	ReleaseDC(hwnd, s.hdc);
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(s.hglrc);
}


// Initialize all user-defined stuff
void readRegistry(){
	auto& s = state();
	LONG result;
	HKEY skey;
	DWORD val;

	setDefaults();

	result = RegOpenKeyEx(HKEY_CURRENT_USER, registryPath, 0, KEY_READ, &skey);
	if(result != ERROR_SUCCESS)
		return;


	if(rssaver::readRegistryDWORD(skey, "NumStars", val))
		s.dNumStars = starfieldSettings::clampToRange(val, starfieldSettings::kNumStars);
	if(rssaver::readRegistryDWORD(skey, "Speed", val))
		s.dSpeed = starfieldSettings::clampToRange(val, starfieldSettings::kSpeed);
	if(rssaver::readRegistryDWORD(skey, "StarSize", val))
		s.dStarSize = starfieldSettings::clampToRange(val, starfieldSettings::kStarSize);
	if(rssaver::readRegistryDWORD(skey, "FrameRateLimit", val))
		dFrameRateLimit = rsWin32Saver::clampFrameRateLimit(val);

	RegCloseKey(skey);
}


// Save all user-defined stuff
void writeRegistry(){
	auto& s = state();
    LONG result;
	HKEY skey;
	DWORD val, disp;

	result = RegCreateKeyEx(HKEY_CURRENT_USER, registryPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &skey, &disp);
	if(result != ERROR_SUCCESS)
		return;

	val = s.dNumStars;
	RegSetValueEx(skey, "NumStars", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = s.dSpeed;
	RegSetValueEx(skey, "Speed", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = s.dStarSize;
	RegSetValueEx(skey, "StarSize", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dFrameRateLimit;
	RegSetValueEx(skey, "FrameRateLimit", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));

	RegCloseKey(skey);
}


INT_PTR CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm){
	switch(msg){
	case WM_CTLCOLORSTATIC:
		if(HWND(lpm) == GetDlgItem(hdlg, WEBPAGE)){
			SetTextColor(HDC(wpm), RGB(0,0,255));
			SetBkColor(HDC(wpm), COLORREF(GetSysColor(COLOR_3DFACE)));
			return (INT_PTR)GetSysColorBrush(COLOR_3DFACE);
		}
		break;
    case WM_COMMAND:
		switch(LOWORD(wpm)){
		case IDOK:
		case IDCANCEL:
			EndDialog(hdlg, LOWORD(wpm));
			break;
		case WEBPAGE:
			ShellExecute(NULL, "open", "http://www.reallyslick.com/", NULL, NULL, SW_SHOWNORMAL);
		}
	}
	return FALSE;
}


// The FPS value is only meaningful while the limit is switched on
void enableFrameRateControls(HWND hdlg, bool enabled){
	EnableWindow(GetDlgItem(hdlg, FRAMERATELIMITEDIT), enabled);
	EnableWindow(GetDlgItem(hdlg, FRAMERATELIMITSPIN), enabled);
}


void initControls(HWND hdlg){
	auto& s = state();
	char cval[16];

	SendDlgItemMessage(hdlg, NUMSTARS, TBM_SETRANGE, 0,
		LPARAM(MAKELONG(DWORD(starfieldSettings::kNumStars.lo), DWORD(starfieldSettings::kNumStars.hi))));
	SendDlgItemMessage(hdlg, NUMSTARS, TBM_SETPOS, 1, LPARAM(s.dNumStars));
	SendDlgItemMessage(hdlg, NUMSTARS, TBM_SETLINESIZE, 0, LPARAM(100));
	SendDlgItemMessage(hdlg, NUMSTARS, TBM_SETPAGESIZE, 0, LPARAM(500));
	sprintf_s(cval, "%d", s.dNumStars);
	SendDlgItemMessage(hdlg, NUMSTARSTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, SPEED, TBM_SETRANGE, 0,
		LPARAM(MAKELONG(DWORD(starfieldSettings::kSpeed.lo), DWORD(starfieldSettings::kSpeed.hi))));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETPOS, 1, LPARAM(s.dSpeed));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETPAGESIZE, 0, LPARAM(5));
	sprintf_s(cval, "%d", s.dSpeed);
	SendDlgItemMessage(hdlg, SPEEDTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, STARSIZE, TBM_SETRANGE, 0,
		LPARAM(MAKELONG(DWORD(starfieldSettings::kStarSize.lo), DWORD(starfieldSettings::kStarSize.hi))));
	SendDlgItemMessage(hdlg, STARSIZE, TBM_SETPOS, 1, LPARAM(s.dStarSize));
	SendDlgItemMessage(hdlg, STARSIZE, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, STARSIZE, TBM_SETPAGESIZE, 0, LPARAM(1));
	sprintf_s(cval, "%d", s.dStarSize);
	SendDlgItemMessage(hdlg, STARSIZETEXT, WM_SETTEXT, 0, LPARAM(cval));

	// dFrameRateLimit keeps its stored meaning, where 0 is unlimited
	const starfieldSettings::FrameRateUi fr = starfieldSettings::frameRateToUi(dFrameRateLimit);
	CheckDlgButton(hdlg, FRAMERATELIMITCHECK, fr.limited ? BST_CHECKED : BST_UNCHECKED);
	SendDlgItemMessage(hdlg, FRAMERATELIMITSPIN, UDM_SETRANGE, 0,
		LPARAM(MAKELONG(DWORD(starfieldSettings::kFrameRate.hi), DWORD(starfieldSettings::kFrameRate.lo))));
	SendDlgItemMessage(hdlg, FRAMERATELIMITSPIN, UDM_SETPOS, 0, LPARAM(fr.fps));
	enableFrameRateControls(hdlg, fr.limited);
}


INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg,
										 WPARAM wpm, LPARAM lpm){
	int ival;
	char cval[16];

    switch(msg){
    case WM_INITDIALOG:
        InitCommonControls();
        readRegistry();
		initControls(hdlg);
        return TRUE;
    case WM_COMMAND:
        switch(LOWORD(wpm)){
        case IDOK:
			state().dNumStars = SendDlgItemMessage(hdlg, NUMSTARS, TBM_GETPOS, 0, 0);
			state().dSpeed = SendDlgItemMessage(hdlg, SPEED, TBM_GETPOS, 0, 0);
			state().dStarSize = SendDlgItemMessage(hdlg, STARSIZE, TBM_GETPOS, 0, 0);
			dFrameRateLimit = starfieldSettings::frameRateFromUi(
				IsDlgButtonChecked(hdlg, FRAMERATELIMITCHECK) == BST_CHECKED,
				int(SendDlgItemMessage(hdlg, FRAMERATELIMITSPIN, UDM_GETPOS, 0, 0)));
			writeRegistry();
            // Fall through
        case IDCANCEL:
            EndDialog(hdlg, LOWORD(wpm));
            break;
		case DEFAULTS:
			setDefaults();
			initControls(hdlg);
			break;
		case FRAMERATELIMITCHECK:
			enableFrameRateControls(hdlg,
				IsDlgButtonChecked(hdlg, FRAMERATELIMITCHECK) == BST_CHECKED);
			break;
		case ABOUT:
			DialogBox(mainInstance, MAKEINTRESOURCE(DLG_ABOUT), hdlg, aboutProc);
		}
        return TRUE;
	case WM_HSCROLL:
		if(HWND(lpm) == GetDlgItem(hdlg, NUMSTARS)){
			ival = SendDlgItemMessage(hdlg, NUMSTARS, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, NUMSTARSTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, SPEED)){
			ival = SendDlgItemMessage(hdlg, SPEED, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, SPEEDTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, STARSIZE)){
			ival = SendDlgItemMessage(hdlg, STARSIZE, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, STARSIZETEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		return TRUE;
    }
    return FALSE;
}


LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm){
	switch(msg){
	case WM_CREATE:
		readRegistry();
		initSaver(hwnd);
		state().readyToDraw = 1;
		break;
	case WM_DESTROY:
		state().readyToDraw = 0;
		cleanUp(hwnd);
		break;
	}
	return defScreenSaverProc(hwnd, msg, wpm, lpm);
}
#endif // WIN32
