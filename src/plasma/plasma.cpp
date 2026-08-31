/*
 * Copyright (C) 1999-2010  Terence M. Welsh
 *
 * This file is part of Plasma.
 *
 * Plasma is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Plasma is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// Plasma screen saver

#ifdef WIN32
#include <windows.h>
#include <rsWin32Saver/rsWin32Saver.h>
#include <rsWin32Saver/rsWin32SaverSettings.h>
#include <process.h>
#include <time.h>
#include <regstr.h>
#include <commctrl.h>
#include "resource.h"
#include "plasmaSettings.h"
#include "../common/saverRegistry.h"
#endif
#ifdef RS_XSCREENSAVER
#include <rsXScreenSaver/rsXScreenSaver.h>
#endif

#include <stdio.h>
#include <math.h>
#include <rsText/rsText.h>
#include <rsMath/rsMath.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "plasmaState.h"

#define PIx2 6.28318530718f

// Globals
#ifdef WIN32
// The one global the framework mandates: rsWin32Saver.h declares it extern and
// reads it back. Everything else this module owns is in plasmaState::State.
LPCTSTR registryPath = ("Software\\Really Slick\\Plasma");
#endif

plasmaState::State& plasmaState::state(){
	static State s;
	return s;
}

using plasmaState::state;
using plasmaState::kNumConsts;
using plasmaState::kTexSize;


// Find absolute value and truncate to 1.0
inline float fabstrunc(float f){
	if(f >= 0.0f)
		return(f <= 1.0f ? f : 1.0f);
	else
		return(f >= -1.0f ? -f : 1.0f);
}



void draw(){
	auto& s = state();
	int i, j;
	float rgb[3];
	float temp;
	static float focus = float(s.dFocus) / 50.0f + 0.3f;
	static float maxdiff = 0.004f * float(s.dSpeed);
	static int index;

	//Update constants
	for(i=0; i<kNumConsts; i++){
		s.ct[i] += s.cv[i];
		if(s.ct[i] > PIx2)
			s.ct[i] -= PIx2;
		s.c[i] = sinf(s.ct[i]) * focus;
	}

	// Update colors
	for(i=0; i<s.plasmasize; i++){
		for(j=0; j<int(float(s.plasmasize) / s.aspectRatio); j++){
			// Calculate vertex colors
			rgb[0] = s.plasma[i][j][0];
			rgb[1] = s.plasma[i][j][1];
			rgb[2] = s.plasma[i][j][2];
			s.plasma[i][j][0] = 0.7f
							* (s.c[0] * s.position[i][j][0] + s.c[1] * s.position[i][j][1]
							+ s.c[2] * (s.position[i][j][0] * s.position[i][j][0] + 1.0f)
							+ s.c[3] * s.position[i][j][0] * s.position[i][j][1]
							+ s.c[4] * rgb[1] + s.c[5] * rgb[2]);
			s.plasma[i][j][1] = 0.7f
							* (s.c[6] * s.position[i][j][0] + s.c[7] * s.position[i][j][1]
							+ s.c[8] * s.position[i][j][0] * s.position[i][j][0]
							+ s.c[9] * (s.position[i][j][1] * s.position[i][j][1] - 1.0f)
							+ s.c[10] * rgb[0] + s.c[11] * rgb[2]);
			s.plasma[i][j][2] = 0.7f
							* (s.c[12] * s.position[i][j][0] + s.c[13] * s.position[i][j][1]
							+ s.c[14] * (1.0f - s.position[i][j][0] * s.position[i][j][1])
							+ s.c[15] * s.position[i][j][1] * s.position[i][j][1]
							+ s.c[16] * rgb[0] + s.c[17] * rgb[1]);

			// Don't let the colors change too much
			temp = s.plasma[i][j][0] - rgb[0];
			if(temp > maxdiff)
				s.plasma[i][j][0] = rgb[0] + maxdiff;
			if(temp < -maxdiff)
				s.plasma[i][j][0] = rgb[0] - maxdiff;
			temp = s.plasma[i][j][1] - rgb[1];
			if(temp > maxdiff)
				s.plasma[i][j][1] = rgb[1] + maxdiff;
			if(temp < -maxdiff)
				s.plasma[i][j][1] = rgb[1] - maxdiff;
			temp = s.plasma[i][j][2] - rgb[2];
			if(temp > maxdiff)
				s.plasma[i][j][2] = rgb[2] + maxdiff;
			if(temp < -maxdiff)
				s.plasma[i][j][2] = rgb[2] - maxdiff;

			// Put colors into texture
			index = (i * kTexSize + j) * 3;
			s.plasmamap[index] = fabstrunc(s.plasma[i][j][0]);
			s.plasmamap[index+1] = fabstrunc(s.plasma[i][j][1]);
			s.plasmamap[index+2] = fabstrunc(s.plasma[i][j][2]);
		}
	}

	// Update texture
	glPixelStorei(GL_UNPACK_ROW_LENGTH, kTexSize);
	glBindTexture(GL_TEXTURE_2D, s.tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, int(float(s.plasmasize) / s.aspectRatio), s.plasmasize,
		GL_RGB, GL_FLOAT, s.plasmamap);

	// Draw it
	// The "- 1" cuts off right and top edges to get rid of blending to black
	float texright = float(s.plasmasize - 1) / float(kTexSize);
	float textop = float(int(float(s.plasmasize) / s.aspectRatio) - 1) / float(kTexSize);
	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2f(0.0f, 0.0f);
		glVertex2f(0.0f, 0.0f);
		glTexCoord2f(0.0f, texright);
		glVertex2f(1.0f, 0.0f);
		glTexCoord2f(textop, 0.0f);
		glVertex2f(0.0f, 1.0f);
		glTexCoord2f(textop, texright);
		glVertex2f(1.0f, 1.0f);
	glEnd();

	// print text
	static float totalTime = 0.0f;
	totalTime += s.frameTime;
	static std::string str;
	static int frames = 0;
	++frames;
	if(frames == 20){
		str = "FPS = " + to_string(20.0f / totalTime);
		totalTime = 0.0f;
		frames = 0;
	}
	if(kStatistics && s.textwriter){
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


void setPlasmaSize(){
	auto& s = state();
	if(s.aspectRatio >= 1.0f){
		s.wide = 30.0f / float(s.dZoom);
		s.high = s.wide / s.aspectRatio;
	}
	else{
		s.high = 30.0f / float(s.dZoom);
		s.wide = s.high * s.aspectRatio;
	}

	// Set resolution of plasma
	if(s.aspectRatio >= 1.0f)
		s.plasmasize = int(float(s.dResolution * kTexSize) * 0.01f);
	else
		s.plasmasize = int(float(s.dResolution * kTexSize) * s.aspectRatio * 0.01f);

	for(int i=0; i<s.plasmasize; i++){
		for(int j=0; j<int(float(s.plasmasize) / s.aspectRatio); j++){
			s.plasma[i][j][0] = 0.0f;
			s.plasma[i][j][1] = 0.0f;
			s.plasma[i][j][2] = 0.0f;
			s.position[i][j][0] = float(i * s.wide) / float(s.plasmasize - 1) - (s.wide * 0.5f);
			s.position[i][j][1] = float(j * s.high) / (float(s.plasmasize) / s.aspectRatio - 1.0f) - (s.high * 0.5f);
		}
	}
}


void setDefaults(){
	auto& s = state();
	s.dZoom = 10;
	s.dFocus = 30;
	s.dSpeed = 20;
	s.dResolution = 25;
	dFrameRateLimit = 30;
}


#ifdef RS_XSCREENSAVER
void handleCommandLine(int argc, char* argv[]){
	auto& s = state();
	setDefaults();
	getArgumentsValue(argc, argv, std::string("-zoom"), s.dZoom, 1, 100);
	getArgumentsValue(argc, argv, std::string("-focus"), s.dFocus, 1, 100);
	getArgumentsValue(argc, argv, std::string("-speed"), s.dSpeed, 1, 100);
	getArgumentsValue(argc, argv, std::string("-resolution"), s.dResolution, 1, 100);
}

void reshape(int width, int height){
	auto& s = state();
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	s.aspectRatio = float(width) / float(height);
	gluOrtho2D(0.0f, 1.0f, 0.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);

	setPlasmaSize();
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
	s.aspectRatio = float(rect.right) / float(rect.bottom);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0.0f, 1.0f, 0.0f, 1.0f);
#endif
#ifdef RS_XSCREENSAVER
void initSaver(){
	auto& s = state();
#endif
	int i;

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Initialize constants
	for(i=0; i<kNumConsts; i++){
		s.ct[i] = rsRandf(PIx2);
		s.cv[i] = rsRandf(0.005f * float(s.dSpeed)) + 0.0001f;
	}

	// Make texture
	glGenTextures(1, &s.tex);
	glBindTexture(GL_TEXTURE_2D, s.tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, kTexSize, kTexSize, 0,
		GL_RGB, GL_FLOAT, s.plasmamap);
	glEnable(GL_TEXTURE_2D);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

	setPlasmaSize();

	// Initialize text
	s.textwriter = new rsText;

	s.readyToDraw = 1;
}


#ifdef RS_XSCREENSAVER
void cleanUp(){
	;
}
#endif


#ifdef WIN32
void cleanUp(HWND hwnd){
	auto& s = state();
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


	if(rssaver::readRegistryDWORD(skey, "Zoom", val))
		s.dZoom = plasmaSettings::clampToRange(val, plasmaSettings::kZoom);
	if(rssaver::readRegistryDWORD(skey, "Focus", val))
		s.dFocus = plasmaSettings::clampToRange(val, plasmaSettings::kFocus);
	if(rssaver::readRegistryDWORD(skey, "Speed", val))
		s.dSpeed = plasmaSettings::clampToRange(val, plasmaSettings::kSpeed);
	if(rssaver::readRegistryDWORD(skey, "Resolution", val))
		s.dResolution = plasmaSettings::clampToRange(val, plasmaSettings::kResolution);
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

	val = s.dZoom;
	RegSetValueEx(skey, "Zoom", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = s.dFocus;
	RegSetValueEx(skey, "Focus", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = s.dSpeed;
	RegSetValueEx(skey, "Speed", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = s.dResolution;
	RegSetValueEx(skey, "Resolution", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
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


void initControls(HWND hdlg){
	auto& s = state();
	char cval[16];

	SendDlgItemMessage(hdlg, ZOOM, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(1), DWORD(100))));
	SendDlgItemMessage(hdlg, ZOOM, TBM_SETPOS, 1, LPARAM(s.dZoom));
	SendDlgItemMessage(hdlg, ZOOM, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, ZOOM, TBM_SETPAGESIZE, 0, LPARAM(5));
	sprintf_s(cval, "%d", s.dZoom);
	SendDlgItemMessage(hdlg, ZOOMTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, FOCUS, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(1), DWORD(100))));
	SendDlgItemMessage(hdlg, FOCUS, TBM_SETPOS, 1, LPARAM(s.dFocus));
	SendDlgItemMessage(hdlg, FOCUS, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, FOCUS, TBM_SETPAGESIZE, 0, LPARAM(5));
	sprintf_s(cval, "%d", s.dFocus);
	SendDlgItemMessage(hdlg, FOCUSTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, SPEED, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(1), DWORD(100))));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETPOS, 1, LPARAM(s.dSpeed));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETPAGESIZE, 0, LPARAM(5));
	sprintf_s(cval, "%d", s.dSpeed);
	SendDlgItemMessage(hdlg, SPEEDTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, RESOLUTION, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(1), DWORD(100))));
	SendDlgItemMessage(hdlg, RESOLUTION, TBM_SETPOS, 1, LPARAM(s.dResolution));
	SendDlgItemMessage(hdlg, RESOLUTION, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, RESOLUTION, TBM_SETPAGESIZE, 0, LPARAM(5));
	sprintf_s(cval, "%d", s.dResolution);
	SendDlgItemMessage(hdlg, RESOLUTIONTEXT, WM_SETTEXT, 0, LPARAM(cval));

	initFrameRateLimitSlider(hdlg, FRAMERATELIMIT, FRAMERATELIMITTEXT);
}


INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg,
										 WPARAM wpm, LPARAM lpm){
	auto& s = state();
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
			s.dZoom = SendDlgItemMessage(hdlg, ZOOM, TBM_GETPOS, 0, 0);
			s.dFocus = SendDlgItemMessage(hdlg, FOCUS, TBM_GETPOS, 0, 0);
			s.dSpeed = SendDlgItemMessage(hdlg, SPEED, TBM_GETPOS, 0, 0);
			s.dResolution = SendDlgItemMessage(hdlg, RESOLUTION, TBM_GETPOS, 0, 0);
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
			DialogBox(mainInstance, MAKEINTRESOURCE(DLG_ABOUT), hdlg, aboutProc);
		}
        return TRUE;
	case WM_HSCROLL:
		if(HWND(lpm) == GetDlgItem(hdlg, ZOOM)){
			ival = SendDlgItemMessage(hdlg, ZOOM, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, ZOOMTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, FOCUS)){
			ival = SendDlgItemMessage(hdlg, FOCUS, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, FOCUSTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, SPEED)){
			ival = SendDlgItemMessage(hdlg, SPEED, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, SPEEDTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, RESOLUTION)){
			ival = SendDlgItemMessage(hdlg, RESOLUTION, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, RESOLUTIONTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, FRAMERATELIMIT))
			updateFrameRateLimitSlider(hdlg, FRAMERATELIMIT, FRAMERATELIMITTEXT);
		return TRUE;
    }
    return FALSE;
}


LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm){
	auto& s = state();
	switch(msg){
	case WM_CREATE:
		readRegistry();
		initSaver(hwnd);
		s.readyToDraw = 1;
		break;
	case WM_DESTROY:
		s.readyToDraw = 0;
		cleanUp(hwnd);
		break;
	}
	return defScreenSaverProc(hwnd, msg, wpm, lpm);
}
#endif // WIN32
