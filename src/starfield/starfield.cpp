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
#include <process.h>
#include <time.h>
#include <regstr.h>
#include <commctrl.h>
#include <resource.h>
#endif
#ifdef RS_XSCREENSAVER
#include <rsXScreenSaver/rsXScreenSaver.h>
#endif

#include <stdio.h>
#include <math.h>
#include <rsText/rsText.h>
#include <GL/gl.h>
#include <GL/glu.h>


// Globals
#ifdef WIN32
LPCTSTR registryPath = ("Software\\Really Slick\\Starfield");
HGLRC hglrc;
HDC hdc;
#endif
int readyToDraw = 0;
float frameTime = 0.0f;
float aspectRatio;

// Star data (heap-allocated arrays)
float* starX = NULL;
float* starY = NULL;
float* starZ = NULL;
float* starV = NULL;  // per-star velocity multiplier

// text output
rsText* textwriter;

// Parameters edited in the dialog box
int dNumStars;
int dSpeed;
int dStarSize;

// Constants
const float farZ = 200.0f;
const float nearZ = 0.1f;
const float fovHalfTan = 0.41421356f;  // tan(22.5 degrees) for 45 degree vertical FOV


// Useful random number macros
// Don't forget to initialize with srand()
inline int rsRandi(int x){
	return rand() % x;
}
inline float rsRandf(float x){
	return x * (float(rand()) / float(RAND_MAX));
}


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

	// Update and render stars
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
		float size = float(dStarSize) * brightness;
		if(size < 1.0f) size = 1.0f;
		glPointSize(size);
		glColor3f(brightness, brightness, brightness);
		glBegin(GL_POINTS);
		glVertex3f(starX[i], starY[i], -starZ[i]);
		glEnd();
	}

	// print text
	static float totalTime = 0.0f;
	totalTime += frameTime;
	static std::string str;
	static int frames = 0;
	++frames;
	if(frames == 20){
		str = "FPS = " + to_string(20.0f / totalTime);
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
	dNumStars = 2000;
	dSpeed = 50;
	dStarSize = 1;
	dFrameRateLimit = 0;
}


void allocateStars(){
	delete[] starX;
	delete[] starY;
	delete[] starZ;
	delete[] starV;
	starX = new float[dNumStars];
	starY = new float[dNumStars];
	starZ = new float[dNumStars];
	starV = new float[dNumStars];
	for(int i = 0; i < dNumStars; i++)
		initStar(i);
}


#ifdef RS_XSCREENSAVER
void handleCommandLine(int argc, char* argv[]){
	setDefaults();
	getArgumentsValue(argc, argv, std::string("-numstars"), dNumStars, 100, 10000);
	getArgumentsValue(argc, argv, std::string("-speed"), dSpeed, 1, 100);
	getArgumentsValue(argc, argv, std::string("-starsize"), dStarSize, 1, 10);
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
	aspectRatio = float(rect.right) / float(rect.bottom);
#endif
#ifdef RS_XSCREENSAVER
void initSaver(){
#endif

	srand((unsigned)time(NULL));

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
	textwriter = new rsText;

	readyToDraw = 1;
}


#ifdef RS_XSCREENSAVER
void cleanUp(){
	;
}
#endif


#ifdef WIN32
void cleanUp(HWND hwnd){
	delete[] starX; starX = NULL;
	delete[] starY; starY = NULL;
	delete[] starZ; starZ = NULL;
	delete[] starV; starV = NULL;
	delete textwriter;
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
		dNumStars = val;
	result = RegQueryValueEx(skey, "Speed", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dSpeed = val;
	result = RegQueryValueEx(skey, "StarSize", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dStarSize = val;
	result = RegQueryValueEx(skey, "FrameRateLimit", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dFrameRateLimit = val;

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


BOOL aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm){
	switch(msg){
	case WM_CTLCOLORSTATIC:
		if(HWND(lpm) == GetDlgItem(hdlg, WEBPAGE)){
			SetTextColor(HDC(wpm), RGB(0,0,255));
			SetBkColor(HDC(wpm), COLORREF(GetSysColor(COLOR_3DFACE)));
			return(int(GetSysColorBrush(COLOR_3DFACE)));
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


void initControls(HWND hdlg){
	char cval[16];

	SendDlgItemMessage(hdlg, NUMSTARS, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(100), DWORD(10000))));
	SendDlgItemMessage(hdlg, NUMSTARS, TBM_SETPOS, 1, LPARAM(dNumStars));
	SendDlgItemMessage(hdlg, NUMSTARS, TBM_SETLINESIZE, 0, LPARAM(100));
	SendDlgItemMessage(hdlg, NUMSTARS, TBM_SETPAGESIZE, 0, LPARAM(500));
	sprintf_s(cval, "%d", dNumStars);
	SendDlgItemMessage(hdlg, NUMSTARSTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, SPEED, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(1), DWORD(100))));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETPOS, 1, LPARAM(dSpeed));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETPAGESIZE, 0, LPARAM(5));
	sprintf_s(cval, "%d", dSpeed);
	SendDlgItemMessage(hdlg, SPEEDTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, STARSIZE, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(1), DWORD(10))));
	SendDlgItemMessage(hdlg, STARSIZE, TBM_SETPOS, 1, LPARAM(dStarSize));
	SendDlgItemMessage(hdlg, STARSIZE, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, STARSIZE, TBM_SETPAGESIZE, 0, LPARAM(1));
	sprintf_s(cval, "%d", dStarSize);
	SendDlgItemMessage(hdlg, STARSIZETEXT, WM_SETTEXT, 0, LPARAM(cval));

	initFrameRateLimitSlider(hdlg, FRAMERATELIMIT, FRAMERATELIMITTEXT);
}


BOOL screenSaverConfigureDialog(HWND hdlg, UINT msg,
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
			dFrameRateLimit = SendDlgItemMessage(hdlg, FRAMERATELIMIT, TBM_GETPOS, 0, 0);
			writeRegistry();
            // Fall through
        case IDCANCEL:
            EndDialog(hdlg, LOWORD(wpm));
            break;
		case DEFAULTS:
			setDefaults();
			initControls(hdlg);
			break;
		case ABOUT:
			DialogBox(mainInstance, MAKEINTRESOURCE(DLG_ABOUT), hdlg, DLGPROC(aboutProc));
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
		if(HWND(lpm) == GetDlgItem(hdlg, FRAMERATELIMIT))
			updateFrameRateLimitSlider(hdlg, FRAMERATELIMIT, FRAMERATELIMITTEXT);
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
