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


// Globals
#ifdef WIN32
LPCTSTR registryPath = TEXT("Software\\Really Slick\\Starfield");
HGLRC hglrc;
HDC hdc;
#endif
int readyToDraw = 0;
float frameTime = 0.0f;
float aspectRatio;

// Star data
std::vector<float> starX;
std::vector<float> starY;
std::vector<float> starZ;
std::vector<float> starV;  // per-star velocity multiplier

// text output
std::unique_ptr<rsText> textwriter;

// Parameters edited in the dialog box
int dNumStars;
int dSpeed;
int dStarSize;

// Constants
const float farZ = 200.0f;
const float nearZ = 0.1f;
const float fovHalfTan = 0.41421356f;  // tan(22.5 degrees) for 45 degree vertical FOV
const int maxStarSize = 10;  // matches STARSIZE slider and -starsize CLI range


void initStar(int i){
	starZ[i] = rsRandf(farZ - nearZ) + nearZ;
	float halfH = fovHalfTan * starZ[i];  // visible half-height at this depth
	float halfW = halfH * aspectRatio;    // visible half-width at this depth
	starX[i] = rsRandf(halfW * 2.0f) - halfW;
	starY[i] = rsRandf(halfH * 2.0f) - halfH;
	starV[i] = rsRandf(3.0f) + 0.15f;  // velocity multiplier 0.15 to 3.15
}


void draw(){
	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	float baseSpeed = float(dSpeed) * 0.5f;  // scaled speed

	// Stars are grouped by rounded point size so each size needs only one
	// glBegin/glEnd pair per frame instead of one per star.
	static std::array<std::vector<int>, maxStarSize + 1> sizeBuckets;  // index 0 unused
	static std::vector<float> starBrightness;
	if(starBrightness.size() != size_t(dNumStars))
		starBrightness.resize(dNumStars);
	for(auto& bucket : sizeBuckets)
		bucket.clear();

	// Update stars
	for(int i = 0; i < dNumStars; i++){
		// Move star toward viewer
		starZ[i] -= baseSpeed * starV[i] * frameTime;

		// Respawn if past viewer or outside visible frustum
		float halfH = fovHalfTan * starZ[i];
		float halfW = halfH * aspectRatio;
		if(starZ[i] < nearZ || starX[i] < -halfW || starX[i] > halfW
			|| starY[i] < -halfH || starY[i] > halfH){
			starZ[i] = farZ - rsRandf(10.0f);  // respawn near far plane
			halfH = fovHalfTan * starZ[i];
			halfW = halfH * aspectRatio;
			starX[i] = rsRandf(halfW * 2.0f) - halfW;
			starY[i] = rsRandf(halfH * 2.0f) - halfH;
			starV[i] = rsRandf(3.0f) + 0.15f;
		}

		// Brightness and size based on distance (closer = brighter and bigger)
		float brightness = 1.0f - (starZ[i] / farZ);
		if(brightness < 0.0f) brightness = 0.0f;
		if(brightness > 1.0f) brightness = 1.0f;
		starBrightness[i] = brightness;

		// Bucket by rounded size. NaN fails every comparison, so the range
		// test is written as "is in range" rather than "is out of range":
		// anything that is not - NaN included - falls to the minimum instead
		// of slipping past two ifs and indexing sizeBuckets far out of
		// bounds. Task 12 in docs/MAINTENANCE.md has how that was reachable.
		const float size = float(dStarSize) * brightness;
		int bucket = 1;
		if(size >= 1.0f)
			bucket = (size <= float(maxStarSize)) ? int(size + 0.5f) : maxStarSize;
		sizeBuckets[bucket].push_back(i);
	}

	// Render stars, one batch per point size
	for(int s = 1; s <= maxStarSize; s++){
		if(sizeBuckets[s].empty()) continue;
		glPointSize(float(s));
		glBegin(GL_POINTS);
		for(int idx : sizeBuckets[s]){
			float b = starBrightness[idx];
			glColor3f(b, b, b);
			glVertex3f(starX[idx], starY[idx], -starZ[idx]);
		}
		glEnd();
	}

	// print text
	static float totalTime = 0.0f;
	totalTime += frameTime;
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
		glOrtho(0.0f, 50.0f * aspectRatio, 0.0f, 50.0f, -1.0f, 1.0f);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glTranslatef(1.0f, 48.0f, 0.0f);

		glColor3f(1.0f, 0.6f, 0.0f);
		textwriter->draw(str);

		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
	}

#ifdef WIN32
	wglSwapLayerBuffers(hdc, WGL_SWAP_MAIN_PLANE);
#endif
#ifdef RS_XSCREENSAVER
	glXSwapBuffers(xdisplay, xwindow);
#endif
}


void idleProc(){
	// update time
	static rsTimer timer;
	frameTime = float(timer.tick());

#ifdef RS_XSCREENSAVER
	const bool shouldDraw = (readyToDraw && !isSuspended && !checkingPassword);
#else
	const bool shouldDraw = (readyToDraw && !isSuspended);
#endif
	if(shouldDraw)
		draw();
}


void setDefaults(){
	dNumStars = starfieldSettings::kDefaultNumStars;
	dSpeed = starfieldSettings::kDefaultSpeed;
	dStarSize = starfieldSettings::kDefaultStarSize;
	dFrameRateLimit = 0;  // unlimited
}


void allocateStars(){
	starX.resize(dNumStars);
	starY.resize(dNumStars);
	starZ.resize(dNumStars);
	starV.resize(dNumStars);
	for(int i = 0; i < dNumStars; i++)
		initStar(i);
}


#ifdef RS_XSCREENSAVER
void handleCommandLine(int argc, char* argv[]){
	setDefaults();
	getArgumentsValue(argc, argv, std::string("-numstars"), dNumStars,
		starfieldSettings::kNumStars.lo, starfieldSettings::kNumStars.hi);
	getArgumentsValue(argc, argv, std::string("-speed"), dSpeed,
		starfieldSettings::kSpeed.lo, starfieldSettings::kSpeed.hi);
	getArgumentsValue(argc, argv, std::string("-starsize"), dStarSize,
		starfieldSettings::kStarSize.lo, starfieldSettings::kStarSize.hi);
}

void reshape(int width, int height){
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	aspectRatio = float(width) / float(height);
	gluPerspective(45.0, double(aspectRatio), double(nearZ), double(farZ));
	glMatrixMode(GL_MODELVIEW);
}
#endif


#ifdef WIN32
void initSaver(HWND hwnd){
	RECT rect;

	// Window initialization
	hdc = GetDC(hwnd);
	setBestPixelFormat(hdc);
	hglrc = wglCreateContext(hdc);
	GetClientRect(hwnd, &rect);
	wglMakeCurrent(hdc, hglrc);
	glViewport(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
	aspectRatio = float(rect.right - rect.left) / float(rect.bottom - rect.top);
#endif
#ifdef RS_XSCREENSAVER
void initSaver(){
#endif

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_POINT_SMOOTH);
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);

	// Perspective projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0, double(aspectRatio), double(nearZ), double(farZ));

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	allocateStars();

	// Initialize text
	textwriter = std::make_unique<rsText>();

	readyToDraw = 1;
}


void freeStars(){
	starX.clear(); starX.shrink_to_fit();
	starY.clear(); starY.shrink_to_fit();
	starZ.clear(); starZ.shrink_to_fit();
	starV.clear(); starV.shrink_to_fit();
}


#ifdef RS_XSCREENSAVER
void cleanUp(){
	freeStars();
	textwriter.reset();
}
#endif


#ifdef WIN32
void cleanUp(HWND hwnd){
	freeStars();
	textwriter.reset();
	// Kill device context
	ReleaseDC(hwnd, hdc);
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hglrc);
}


// Initialize all user-defined stuff
void readRegistry(){
	LONG result;
	HKEY skey;
	DWORD valtype, valsize, val;

	setDefaults();

	result = RegOpenKeyEx(HKEY_CURRENT_USER, registryPath, 0, KEY_READ, &skey);
	if(result != ERROR_SUCCESS)
		return;

	valsize=sizeof(val);

	result = RegQueryValueEx(skey, "NumStars", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dNumStars = starfieldSettings::clampToRange(val, starfieldSettings::kNumStars);
	result = RegQueryValueEx(skey, "Speed", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dSpeed = starfieldSettings::clampToRange(val, starfieldSettings::kSpeed);
	result = RegQueryValueEx(skey, "StarSize", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dStarSize = starfieldSettings::clampToRange(val, starfieldSettings::kStarSize);
	result = RegQueryValueEx(skey, "FrameRateLimit", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dFrameRateLimit = rsWin32Saver::clampFrameRateLimit(val);

	RegCloseKey(skey);
}


// Save all user-defined stuff
void writeRegistry(){
    LONG result;
	HKEY skey;
	DWORD val, disp;

	result = RegCreateKeyEx(HKEY_CURRENT_USER, registryPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &skey, &disp);
	if(result != ERROR_SUCCESS)
		return;

	val = dNumStars;
	RegSetValueEx(skey, "NumStars", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dSpeed;
	RegSetValueEx(skey, "Speed", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dStarSize;
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
	char cval[16];

	SendDlgItemMessage(hdlg, NUMSTARS, TBM_SETRANGE, 0,
		LPARAM(MAKELONG(DWORD(starfieldSettings::kNumStars.lo), DWORD(starfieldSettings::kNumStars.hi))));
	SendDlgItemMessage(hdlg, NUMSTARS, TBM_SETPOS, 1, LPARAM(dNumStars));
	SendDlgItemMessage(hdlg, NUMSTARS, TBM_SETLINESIZE, 0, LPARAM(100));
	SendDlgItemMessage(hdlg, NUMSTARS, TBM_SETPAGESIZE, 0, LPARAM(500));
	sprintf_s(cval, "%d", dNumStars);
	SendDlgItemMessage(hdlg, NUMSTARSTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, SPEED, TBM_SETRANGE, 0,
		LPARAM(MAKELONG(DWORD(starfieldSettings::kSpeed.lo), DWORD(starfieldSettings::kSpeed.hi))));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETPOS, 1, LPARAM(dSpeed));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETPAGESIZE, 0, LPARAM(5));
	sprintf_s(cval, "%d", dSpeed);
	SendDlgItemMessage(hdlg, SPEEDTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, STARSIZE, TBM_SETRANGE, 0,
		LPARAM(MAKELONG(DWORD(starfieldSettings::kStarSize.lo), DWORD(starfieldSettings::kStarSize.hi))));
	SendDlgItemMessage(hdlg, STARSIZE, TBM_SETPOS, 1, LPARAM(dStarSize));
	SendDlgItemMessage(hdlg, STARSIZE, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, STARSIZE, TBM_SETPAGESIZE, 0, LPARAM(1));
	sprintf_s(cval, "%d", dStarSize);
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
			dNumStars = SendDlgItemMessage(hdlg, NUMSTARS, TBM_GETPOS, 0, 0);
			dSpeed = SendDlgItemMessage(hdlg, SPEED, TBM_GETPOS, 0, 0);
			dStarSize = SendDlgItemMessage(hdlg, STARSIZE, TBM_GETPOS, 0, 0);
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
		readyToDraw = 1;
		break;
	case WM_DESTROY:
		readyToDraw = 0;
		cleanUp(hwnd);
		break;
	}
	return defScreenSaverProc(hwnd, msg, wpm, lpm);
}
#endif // WIN32
