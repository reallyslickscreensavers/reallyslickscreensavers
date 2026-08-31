/*
 * Copyright (C) 1999-2010  Terence M. Welsh
 *
 * This file is part of Field Lines.
 *
 * Field Lines is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Field Lines is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// Field Lines screensaver


#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include <gl/gl.h>
#include <gl/glu.h>

#include <regstr.h>
#include <commctrl.h>
#include "resource.h"
#include "fieldlinesSettings.h"
#include "fieldlinesState.h"
#include "../common/saverRegistry.h"

#include <rsWin32Saver/rsWin32Saver.h>
#include <rsWin32Saver/rsWin32SaverSettings.h>
#include <rsText/rsText.h>
#include <rsMath/rsMath.h>

#define PIx2 6.28318530718f


class ion;


// The one global the framework mandates: rsWin32Saver.h declares it extern and
// reads it back. Everything else this module owns is in fieldlinesState::State.
LPCTSTR registryPath = ("Software\\Really Slick\\Field Lines");

fieldlinesState::State& fieldlinesState::state(){
	static State s;
	return s;
}

using fieldlinesState::state;


class ion{
public:
	float charge;
	float xyz[3];
	float vel[3];
	float angle;
	float anglevel;

	ion();
	~ion(){};
	void update();
};

ion::ion(){
	auto& s = state();
	if(rsRandi(2))
		charge = -1.0f;
	else
		charge = 1.0f;
	xyz[0] = rsRandf(2.0f * s.wide) - s.wide;
	xyz[1] = rsRandf(2.0f * s.high) - s.high;
	xyz[2] = rsRandf(2.0f * s.deep) - s.deep;
	vel[0] = rsRandf(float(s.dSpeed) * 4.0f) - (float(s.dSpeed) * 2.0f);
	vel[1] = rsRandf(float(s.dSpeed) * 4.0f) - (float(s.dSpeed) * 2.0f);
	vel[2] = rsRandf(float(s.dSpeed) * 4.0f) - (float(s.dSpeed) * 2.0f);
	angle = 0.0f;
	anglevel = 0.0005f * float(s.dSpeed) + 0.0005f * rsRandf(float(s.dSpeed));
}

void ion::update(){
	auto& s = state();
	xyz[0] += vel[0] * s.frameTime;
	xyz[1] += vel[1] * s.frameTime;
	xyz[2] += vel[2] * s.frameTime;
	if(xyz[0] > s.wide)
		vel[0] -= 0.1f * float(s.dSpeed);
	if(xyz[0] < -s.wide)
		vel[0] += 0.1f * float(s.dSpeed);
	if(xyz[1] > s.high)
		vel[1] -= 0.1f * float(s.dSpeed);
	if(xyz[1] < -s.high)
		vel[1] += 0.1f * float(s.dSpeed);
	if(xyz[2] > s.deep)
		vel[2] -= 0.1f * float(s.dSpeed);
	if(xyz[2] < -s.deep)
		vel[2] += 0.1f * float(s.dSpeed);

	angle += anglevel;
	if(angle > PIx2)
		angle -= PIx2;
}


void drawfieldline(int source, float x, float y, float z){
	auto& s = state();
	int i, j;
	float charge;
	float repulsion;
	float dist, distsquared, distrec;
	float xyz[3];
	float lastxyz[3];
	float dir[3];
	float end[3];
	float tempvec[3];
	float r, g, b;
	float lastr, lastg, lastb;
	static float brightness = 10000.0f;

	charge = s.ions[source].charge;
	lastxyz[0] = s.ions[source].xyz[0];
	lastxyz[1] = s.ions[source].xyz[1];
	lastxyz[2] = s.ions[source].xyz[2];
	dir[0] = x;
	dir[1] = y;
	dir[2] = z;

	// Do the first segment
	r = float(fabs(dir[2])) * brightness;
	g = float(fabs(dir[0])) * brightness;
	b = float(fabs(dir[1])) * brightness;
	if(r > 1.0f)
		r = 1.0f;
	if(g > 1.0f)
		g = 1.0f;
	if(b > 1.0f)
		b = 1.0f;
	lastr = r;
	lastg = g;
	lastb = b;
	glColor3f(r, g, b);
	xyz[0] = lastxyz[0] + dir[0];
	xyz[1] = lastxyz[1] + dir[1];
	xyz[2] = lastxyz[2] + dir[2];
	if(s.dElectric){
		xyz[0] += rsRandf(float(s.dStepSize) * 0.2f) - (float(s.dStepSize) * 0.1f);
		xyz[1] += rsRandf(float(s.dStepSize) * 0.2f) - (float(s.dStepSize) * 0.1f);
		xyz[2] += rsRandf(float(s.dStepSize) * 0.2f) - (float(s.dStepSize) * 0.1f);
	}
	if(!s.dConstwidth)
		glLineWidth((xyz[2] + 300.0f) * 0.000333f * float(s.dWidth));
	glBegin(GL_LINE_STRIP);
		glColor3f(lastr, lastg, lastb);
		glVertex3fv(lastxyz);
		glColor3f(r, g, b);
		glVertex3fv(xyz);
	if(!s.dConstwidth)
		glEnd();

	for(i=0; i<int(s.dMaxSteps); i++){
		dir[0] = 0.0f;
		dir[1] = 0.0f;
		dir[2] = 0.0f;
		for(j=0; j<int(s.dIons); j++){
			repulsion = charge * s.ions[j].charge;
			tempvec[0] = xyz[0] - s.ions[j].xyz[0];
			tempvec[1] = xyz[1] - s.ions[j].xyz[1];
			tempvec[2] = xyz[2] - s.ions[j].xyz[2];
			distsquared = tempvec[0] * tempvec[0] + tempvec[1] * tempvec[1] + tempvec[2] * tempvec[2];
			dist = float(sqrt(distsquared));
			if(dist < float(s.dStepSize) && i > 2){
				end[0] = s.ions[j].xyz[0];
				end[1] = s.ions[j].xyz[1];
				end[2] = s.ions[j].xyz[2];
				i = 10000;
			}
			tempvec[0] /= dist;
			tempvec[1] /= dist;
			tempvec[2] /= dist;
			if(distsquared < 1.0f)
				distsquared = 1.0f;
			dir[0] += tempvec[0] * repulsion / distsquared;
			dir[1] += tempvec[1] * repulsion / distsquared;
			dir[2] += tempvec[2] * repulsion / distsquared;
		}
		lastr = r;
		lastg = g;
		lastb = b;
		r = float(fabs(dir[2])) * brightness;
		g = float(fabs(dir[0])) * brightness;
		b = float(fabs(dir[1])) * brightness;
		if(s.dElectric){
			r *= 10.0f;
			g *= 10.0f;
			b *= 10.0f;;
			if(r > b * 0.5f)
				r = b * 0.5f;
			if(g > b * 0.3f)
				g = b * 0.3f;
		}
		if(r > 1.0f)
			r = 1.0f;
		if(g > 1.0f)
			g = 1.0f;
		if(b > 1.0f)
			b = 1.0f;
		distsquared = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
		distrec = float(s.dStepSize) / float(sqrt(distsquared));
		dir[0] *= distrec;
		dir[1] *= distrec;
		dir[2] *= distrec;
		if(s.dElectric){
			dir[0] += rsRandf(float(s.dStepSize)) - (float(s.dStepSize) * 0.5f);
			dir[1] += rsRandf(float(s.dStepSize)) - (float(s.dStepSize) * 0.5f);
			dir[2] += rsRandf(float(s.dStepSize)) - (float(s.dStepSize) * 0.5f);
		}
		lastxyz[0] = xyz[0];
		lastxyz[1] = xyz[1];
		lastxyz[2] = xyz[2];
		xyz[0] += dir[0];
		xyz[1] += dir[1];
		xyz[2] += dir[2];
		if(!s.dConstwidth){
			glLineWidth((xyz[2] + 300.0f) * 0.000333f * float(s.dWidth));
			glBegin(GL_LINE_STRIP);
		}
			glColor3f(lastr, lastg, lastb);
			glVertex3fv(lastxyz);
			if(i != 10000){
				if(i == (int(s.dMaxSteps) - 1))
					glColor3f(0.0f, 0.0f, 0.0f);
				else
					glColor3f(r, g, b);
				glVertex3fv(xyz);
				if(!s.dConstwidth || i == (int(s.dMaxSteps) - 1))
					glEnd();
			}
	}
	if(i == 10001){
			glColor3f(r, g, b);
			glVertex3fv(end);
		glEnd();
	}
}


void draw(){
	auto& s = state();
	int i;

	static float axisStep = float(sqrt(float(s.dStepSize) * float(s.dStepSize) * 0.333f));

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for(i=0; i<s.dIons; i++)
		s.ions[i].update();

	for(i=0; i<s.dIons; i++){
		drawfieldline(i, axisStep, axisStep, axisStep);
		drawfieldline(i, axisStep, axisStep, -axisStep);
		drawfieldline(i, axisStep, -axisStep, axisStep);
		drawfieldline(i, axisStep, -axisStep, -axisStep);
		drawfieldline(i, -axisStep, axisStep, axisStep);
		drawfieldline(i, -axisStep, axisStep, -axisStep);
		drawfieldline(i, -axisStep, -axisStep, axisStep);
		drawfieldline(i, -axisStep, -axisStep, -axisStep);
	}

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
	// textwriter is checked as well as kStatistics; see Task 6 in docs/MAINTENANCE.md.
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

    wglSwapLayerBuffers(s.hdc, WGL_SWAP_MAIN_PLANE);
}


void idleProc(){
	auto& s = state();
	// update time
	static rsTimer timer;
	s.frameTime = float(timer.tick());

	if(s.readyToDraw && !isSuspended)
		draw();
}


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

	// calculate boundaries
	if((rect.right - rect.left) > (rect.bottom - rect.top)){
		s.high = s.deep = 160.0f;
		s.wide = s.high * (rect.right - rect.left) / (rect.bottom - rect.top);
	}
	else{
		s.wide = s.deep = 160.0f;
		s.high = s.wide * (rect.bottom - rect.top) / (rect.right - rect.left);
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LINE_SMOOTH);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	s.aspectRatio = float(rect.right) / float(rect.bottom);
	gluPerspective(60.0, s.aspectRatio, 1.0, s.deep * 10);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0, 0.0, -2.0f * s.deep);

	if(s.dConstwidth)
		glLineWidth(float(s.dWidth) * 0.1f);

	if(s.dIons < 1)
		s.dIons = 1;
	if(s.dIons > 50)
		s.dIons = 50;
	s.ions = new ion[s.dIons];

	// Initialize text
	s.textwriter = new rsText;
}


void cleanUp(HWND hwnd){
	auto& s = state();
	// Free memory
	delete[] s.ions;

	// Kill device context
	ReleaseDC(hwnd, s.hdc);
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(s.hglrc);
}


void setDefaults(){
	auto& s = state();
	s.dIons = 6;
	s.dStepSize = 10;
	s.dMaxSteps = 300;
	s.dWidth = 30;
	s.dSpeed = 10;
	s.dConstwidth = FALSE;
	s.dElectric = FALSE;
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


	if(rssaver::readRegistryDWORD(skey, "Ions", val))
		s.dIons = fieldlinesSettings::clampToRange(val, fieldlinesSettings::kIons);
	if(rssaver::readRegistryDWORD(skey, "StepSize", val))
		s.dStepSize = fieldlinesSettings::clampToRange(val, fieldlinesSettings::kStepSize);
	if(rssaver::readRegistryDWORD(skey, "MaxSteps", val))
		s.dMaxSteps = fieldlinesSettings::clampToRange(val, fieldlinesSettings::kMaxSteps);
	if(rssaver::readRegistryDWORD(skey, "Width", val))
		s.dWidth = fieldlinesSettings::clampToRange(val, fieldlinesSettings::kWidth);
	if(rssaver::readRegistryDWORD(skey, "Speed", val))
		s.dSpeed = fieldlinesSettings::clampToRange(val, fieldlinesSettings::kSpeed);
	if(rssaver::readRegistryDWORD(skey, "Constwidth", val))
		s.dConstwidth = fieldlinesSettings::clampToRange(val, fieldlinesSettings::kConstwidth);
	if(rssaver::readRegistryDWORD(skey, "Electric", val))
		s.dElectric = fieldlinesSettings::clampToRange(val, fieldlinesSettings::kElectric);
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

	val = s.dIons;
	RegSetValueEx(skey, "Ions", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = s.dStepSize;
	RegSetValueEx(skey, "StepSize", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = s.dMaxSteps;
	RegSetValueEx(skey, "MaxSteps", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = s.dWidth;
	RegSetValueEx(skey, "Width", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = s.dSpeed;
	RegSetValueEx(skey, "Speed", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = s.dConstwidth;
	RegSetValueEx(skey, "Constwidth", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = s.dElectric;
	RegSetValueEx(skey, "Electric", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
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
			ShellExecute(NULL, "open", "http://www.reallyslick.com", NULL, NULL, SW_SHOWNORMAL);
		}
	}
	return FALSE;
}


void initControls(HWND hdlg){
	auto& s = state();
	char cval[16];

	SendDlgItemMessage(hdlg, IONS, UDM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(50), DWORD(1))));
	SendDlgItemMessage(hdlg, IONS, UDM_SETPOS, 0, LPARAM(s.dIons));

	SendDlgItemMessage(hdlg, STEPSIZE, UDM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(100), DWORD(1))));
	SendDlgItemMessage(hdlg, STEPSIZE, UDM_SETPOS, 0, LPARAM(s.dStepSize));

	SendDlgItemMessage(hdlg, MAXSTEPS, UDM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(1000), DWORD(1))));
	SendDlgItemMessage(hdlg, MAXSTEPS, UDM_SETPOS, 0, LPARAM(s.dMaxSteps));

	SendDlgItemMessage(hdlg, WIDTH, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(1), DWORD(100))));
	SendDlgItemMessage(hdlg, WIDTH, TBM_SETPOS, 1, LPARAM(s.dWidth));
	SendDlgItemMessage(hdlg, WIDTH, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, WIDTH, TBM_SETPAGESIZE, 0, LPARAM(10));
	sprintf_s(cval, "%d", s.dWidth);
	SendDlgItemMessage(hdlg, WIDTHTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, SPEED, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(1), DWORD(100))));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETPOS, 1, LPARAM(s.dSpeed));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETPAGESIZE, 0, LPARAM(10));
	sprintf_s(cval, "%d", s.dSpeed);
	SendDlgItemMessage(hdlg, SPEEDTEXT, WM_SETTEXT, 0, LPARAM(cval));

	CheckDlgButton(hdlg, CONSTWIDTH, s.dConstwidth);

	CheckDlgButton(hdlg, ELECTRIC, s.dElectric);

	initFrameRateLimitSlider(hdlg, FRAMERATELIMIT, FRAMERATELIMITTEXT);
}


INT_PTR CALLBACK screenSaverConfigureDialog(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm){
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
			s.dIons = SendDlgItemMessage(hdlg, IONS, UDM_GETPOS, 0, 0);
			s.dStepSize = SendDlgItemMessage(hdlg, STEPSIZE, UDM_GETPOS, 0, 0);
			s.dMaxSteps = SendDlgItemMessage(hdlg, MAXSTEPS, UDM_GETPOS, 0, 0);
			s.dWidth = SendDlgItemMessage(hdlg, WIDTH, TBM_GETPOS, 0, 0);
			s.dSpeed = SendDlgItemMessage(hdlg, SPEED, TBM_GETPOS, 0, 0);
			s.dConstwidth = (IsDlgButtonChecked(hdlg, CONSTWIDTH) == BST_CHECKED);
			s.dElectric = (IsDlgButtonChecked(hdlg, ELECTRIC) == BST_CHECKED);
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
		if(HWND(lpm) == GetDlgItem(hdlg, WIDTH)){
			ival = SendDlgItemMessage(hdlg, WIDTH, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, WIDTHTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, SPEED)){
			ival = SendDlgItemMessage(hdlg, SPEED, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, SPEEDTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, FRAMERATELIMIT))
			updateFrameRateLimitSlider(hdlg, FRAMERATELIMIT, FRAMERATELIMITTEXT);
		return TRUE;
    }
    return FALSE;
}


LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm){
	auto& s = state();
	static unsigned long threadID;

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
