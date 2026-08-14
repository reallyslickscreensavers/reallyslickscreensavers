/*
 * Copyright (C) 2026  Really Slick Screensavers Contributors
 *
 * This file is part of Ribbons.
 *
 * Ribbons is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Ribbons is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Ribbons screen saver
//
// Recreates the Windows Vista "Ribbons" screensaver aesthetic:
// smooth, luminous, semi-transparent silk-like ribbons floating
// through 3D space with elegant sweeping motion.

#ifdef WIN32
#include <windows.h>
#include <rsWin32Saver/rsWin32Saver.h>
#include <rsWin32Saver/rsWin32SaverSettings.h>
#include <regstr.h>
#include <commctrl.h>
#include "resource.h"
#endif
#ifdef RS_XSCREENSAVER
#include <rsXScreenSaver/rsXScreenSaver.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <rsText/rsText.h>
#include <Rgbhsl/Rgbhsl.h>
#include <rsMath/rsMath.h>
#include <rsUtility/catmullRom.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "ribbonsSettings.h"


#define PIx2 6.28318530718f
// Number of wide-spaced control points driving each ribbon's path.
#define NUM_CTRL_PTS 16
// Number of interpolated samples between each pair of control points.
#define INTERP_STEPS 18
// Number of harmonic oscillators used for the underlying procedural path.
#define NUM_HARMONICS 4


class Ribbon;


// Global variables
#ifdef WIN32
LPCTSTR registryPath = ("Software\\Really Slick\\Ribbons");
HGLRC hglrc;
HDC hdc;
#endif
float aspectRatio;
float frameTime = 0.0f;
int readyToDraw = 0;
Ribbon *ribbonArray = NULL;
// text output
rsText* textwriter = NULL;
// Parameters edited in the dialog box
int dRibbonCount;
int dRibbonLength;
int dRibbonWidth;
int dSpeed;
int dColorCycling;
int dTransparency;
#ifdef RS_XSCREENSAVER
enum{
	DEFAULTS1,
	DEFAULTS2,
	DEFAULTS3
};
#endif


// Ribbon: a single flowing silk-like band through 3D space.
//
// The path is generated procedurally using a sum of sinusoidal harmonics
// (three per axis) evaluated continuously over time, producing long sweeping
// arcs that traverse the full viewport.  The resulting curve is densely
// sampled with Catmull-Rom interpolation so there are never sharp
// angles.  The ribbon is rendered as a camera-facing triangle strip with
// width that tapers at the head and tail, drawn in multiple additive
// passes to simulate glow and glossy silk highlights.

class Ribbon{
public:
	// Total number of interpolated segments for the visible trail.
	int trailLen;

	// Interpolated position buffer (oldest at tail).
	float (*pos)[3];
	// Smoothed side vectors used to build the triangle strip.
	float (*normals)[3];
	int filled;

	// Coarse control points for Catmull-Rom interpolation.
	float ctrlPts[NUM_CTRL_PTS][3];
	int newestCtrl;
	float interpT;
	float sampleAccumulator;

	// Per-harmonic state for procedural path generation.
	struct Harmonic{
		float phase[3];
		float freq[3];
		float amp[3];
	};
	Harmonic harmonics[NUM_HARMONICS];

	// Color.
	float hue;
	float hueSpeed;
	float baseR, baseG, baseB;

	Ribbon();
	~Ribbon();
	void update(float dt);
	void draw();

private:
	// Evaluate the procedural path at a given time.
	void evalPath(float t, float out[3]);
	void advanceControlPoint();
	void sampleHead(float out[3]) const;
	void pushHeadSample(const float head[3]);

	// Last coarse time value used for generating control points.
	float pathTime;
	// Previous head side vector for orientation continuity.
	float prevSide[3];
	// Off-screen spawn offset that fades out over time.
	float spawnOffset[3];
	float spawnBlend;
};

Ribbon::Ribbon(){
	trailLen = dRibbonLength * INTERP_STEPS;
	pos = new float[trailLen][3];
	normals = new float[trailLen][3];
	filled = 0;
	newestCtrl = NUM_CTRL_PTS - 1;
	interpT = 0.0f;
	sampleAccumulator = 0.0f;
	pathTime = rsRandf(2000.0f);
	spawnBlend = 1.0f;

	hue = rsRandf(1.0f);
	hueSpeed = rsRandf(0.010f) + 0.0025f;

	// Start each ribbon outside the viewport, then fade toward the main field.
	const float spawnAngle = rsRandf(PIx2);
	const float spawnRadius = 26.0f + rsRandf(10.0f);
	spawnOffset[0] = cosf(spawnAngle) * spawnRadius;
	spawnOffset[1] = sinf(spawnAngle) * spawnRadius;
	spawnOffset[2] = (rsRandf(2.0f) - 1.0f) * 4.0f;

	// Low-frequency harmonics produce large, smooth Vista-like sweeps.
	for(int h = 0; h < NUM_HARMONICS; h++){
		float baseFreq = 0.05f + float(h) * 0.08f;
		float baseAmp = 11.0f / (1.0f + float(h) * 1.4f);
		for(int a = 0; a < 3; a++){
			harmonics[h].phase[a] = rsRandf(PIx2);
			harmonics[h].freq[a] = baseFreq * (0.80f + rsRandf(0.40f));
			harmonics[h].amp[a] = baseAmp * (0.75f + rsRandf(0.50f));
		}
		harmonics[h].amp[2] *= 0.33f;
	}

	// Populate initial control points in chronological order.
	for(int i = 0; i < NUM_CTRL_PTS; i++){
		float t = pathTime - float(NUM_CTRL_PTS - 1 - i);
		evalPath(t, ctrlPts[i]);
		if(i > 0)
			ensureMinDist(ctrlPts[i], ctrlPts[i - 1], 0.05f);
	}

	// Initialize trail with the newest control point so startup is stable.
	for(int i = 0; i < trailLen; i++){
		pos[i][0] = ctrlPts[newestCtrl][0];
		pos[i][1] = ctrlPts[newestCtrl][1];
		pos[i][2] = ctrlPts[newestCtrl][2];
		normals[i][0] = 0.0f;
		normals[i][1] = 1.0f;
		normals[i][2] = 0.0f;
	}

	hsl2rgb(hue, 0.92f, 0.54f, baseR, baseG, baseB);

	prevSide[0] = 0.0f;
	prevSide[1] = 1.0f;
	prevSide[2] = 0.0f;
}

Ribbon::~Ribbon(){
	delete[] pos;
	delete[] normals;
}

void Ribbon::evalPath(float t, float out[3]){
	out[0] = out[1] = out[2] = 0.0f;
	for(int h = 0; h < NUM_HARMONICS; h++){
		for(int a = 0; a < 3; a++){
			out[a] += harmonics[h].amp[a] * sinf(harmonics[h].freq[a] * t + harmonics[h].phase[a]);
		}
	}
	out[0] += spawnOffset[0] * spawnBlend;
	out[1] += spawnOffset[1] * spawnBlend;
	out[2] += spawnOffset[2] * spawnBlend;
}

void Ribbon::advanceControlPoint(){
	const int prev = newestCtrl;
	newestCtrl = (newestCtrl + 1) % NUM_CTRL_PTS;
	pathTime += 1.0f;
	evalPath(pathTime, ctrlPts[newestCtrl]);
	ensureMinDist(ctrlPts[newestCtrl], ctrlPts[prev], 0.05f);
	if(spawnBlend > 0.0f){
		spawnBlend *= 0.975f;
		if(spawnBlend < 0.001f)
			spawnBlend = 0.0f;
	}
}

void Ribbon::sampleHead(float out[3]) const{
	const int p3 = newestCtrl;
	const int p2 = (p3 - 1 + NUM_CTRL_PTS) % NUM_CTRL_PTS;
	const int p1 = (p2 - 1 + NUM_CTRL_PTS) % NUM_CTRL_PTS;
	const int p0 = (p1 - 1 + NUM_CTRL_PTS) % NUM_CTRL_PTS;
	for(int a = 0; a < 3; a++){
		out[a] = catmullRom(ctrlPts[p0][a], ctrlPts[p1][a], ctrlPts[p2][a], ctrlPts[p3][a], interpT);
	}
}

void Ribbon::pushHeadSample(const float head[3]){
	for(int i = trailLen - 1; i > 0; i--){
		pos[i][0] = pos[i - 1][0];
		pos[i][1] = pos[i - 1][1];
		pos[i][2] = pos[i - 1][2];
	}
	pos[0][0] = head[0];
	pos[0][1] = head[1];
	pos[0][2] = head[2];
	if(filled < trailLen)
		filled++;
}

void Ribbon::update(float dt){
	if(dt <= 0.0f)
		return;

	// Fixed-rate sampling removes frame-time quantization jitter.
	const float samplesPerSecond = 55.0f + float(dSpeed) * 1.5f;
	sampleAccumulator += dt * samplesPerSecond;
	int steps = int(sampleAccumulator);
	if(steps > 24){
		steps = 24;
		sampleAccumulator = 0.0f;
	} else {
		sampleAccumulator -= float(steps);
	}

	for(int s = 0; s < steps; s++){
		float head[3];
		sampleHead(head);
		if(filled > 0)
			ensureMinDist(head, pos[0], 0.01f);
		pushHeadSample(head);

		interpT += 1.0f / float(INTERP_STEPS);
		if(interpT >= 1.0f){
			interpT -= 1.0f;
			advanceControlPoint();
		}
	}

	hue += hueSpeed * float(dColorCycling) * 0.0018f * dt;
	if(hue > 1.0f) hue -= 1.0f;

	hsl2rgb(hue, 0.92f, 0.54f, baseR, baseG, baseB);
}

void Ribbon::draw(){
	if(filled < 4) return;

	int count = filled;
	float baseWidth = float(dRibbonWidth) * 0.0048f;
	float alpha = float(dTransparency) * 0.01f;

	// Build stable camera-facing side vectors with sign coherence.
	float sx = prevSide[0], sy = prevSide[1], sz = prevSide[2];

	for(int i = 0; i < count; i++){
		int lo = (i > 0) ? i - 1 : i;
		int hi = (i + 1 < count) ? i + 1 : i;
		float tx = pos[lo][0] - pos[hi][0];
		float ty = pos[lo][1] - pos[hi][1];
		float tz = pos[lo][2] - pos[hi][2];
		float tL = sqrtf(tx * tx + ty * ty + tz * tz);
		if(tL > 1e-7f){
			tx /= tL;
			ty /= tL;
			tz /= tL;
		} else {
			tx = 0.0f;
			ty = 0.0f;
			tz = 1.0f;
		}

		float vx = -pos[i][0];
		float vy = -pos[i][1];
		float vz = 20.0f - pos[i][2];
		float vL = sqrtf(vx * vx + vy * vy + vz * vz);
		if(vL > 1e-7f){
			vx /= vL;
			vy /= vL;
			vz /= vL;
		} else {
			vx = 0.0f;
			vy = 0.0f;
			vz = 1.0f;
		}

		float nx = ty * vz - tz * vy;
		float ny = tz * vx - tx * vz;
		float nz = tx * vy - ty * vx;
		float nL = sqrtf(nx * nx + ny * ny + nz * nz);
		if(nL > 1e-7f){
			nx /= nL;
			ny /= nL;
			nz /= nL;
		} else {
			nx = sx;
			ny = sy;
			nz = sz;
		}

		// Keep orientation consistent from segment to segment.
		if(nx * sx + ny * sy + nz * sz < 0.0f){
			nx = -nx;
			ny = -ny;
			nz = -nz;
		}

		sx = nx;
		sy = ny;
		sz = nz;
		normals[i][0] = nx;
		normals[i][1] = ny;
		normals[i][2] = nz;
	}
	prevSide[0] = normals[0][0];
	prevSide[1] = normals[0][1];
	prevSide[2] = normals[0][2];

	for(int i = 1; i < count - 1; i++){
		float mx = normals[i - 1][0] + normals[i][0] + normals[i + 1][0];
		float my = normals[i - 1][1] + normals[i][1] + normals[i + 1][1];
		float mz = normals[i - 1][2] + normals[i][2] + normals[i + 1][2];
		float mL = sqrtf(mx * mx + my * my + mz * mz);
		if(mL > 1e-7f){
			normals[i][0] = mx / mL;
			normals[i][1] = my / mL;
			normals[i][2] = mz / mL;
		}
	}

	// Many closely spaced passes produce a continuous-looking cross-ribbon
	// gradient instead of visible "bands".
	static const int NUM_PASSES = 12;
	for(int pass = 0; pass < NUM_PASSES; pass++){
		const float t = float(pass) / float(NUM_PASSES - 1);  // 0=outer, 1=inner
		const float pw = baseWidth * (2.55f - 2.0f * t);
		const float pa = alpha * (0.015f + 0.24f * t * t);
		const float wm = 0.02f * t * t * t;  // keep center vivid, only slight whitening
		const float im = 0.95f + 0.18f * t;

		// Outer glow additive, inner body alpha-blended for a single solid body.
		if(pass < 8)
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		else
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glBegin(GL_TRIANGLE_STRIP);
		for(int i = 0; i < count; i++){
			float param = float(i) / float(count > 1 ? count - 1 : 1);

			float headTaper = (param < 0.06f) ? (param / 0.06f) : 1.0f;
			headTaper = headTaper * headTaper * (3.0f - 2.0f * headTaper);
			float tailTaper = (param > 0.88f) ? ((1.0f - param) / 0.12f) : 1.0f;
			tailTaper = tailTaper * tailTaper * (3.0f - 2.0f * tailTaper);
			float bodyWave = 0.90f + 0.10f * sinf(param * PIx2);
			float w = pw * headTaper * tailTaper * bodyWave;

			float fadeAlpha = pa * (1.0f - param * 0.80f);

			float nx = normals[i][0], ny = normals[i][1], nz = normals[i][2];

			float cr = (baseR * (1.0f - wm) + wm) * im;
			float cg = (baseG * (1.0f - wm) + wm) * im;
			float cb = (baseB * (1.0f - wm) + wm) * im;
			if(cr > 1.0f) cr = 1.0f;
			if(cg > 1.0f) cg = 1.0f;
			if(cb > 1.0f) cb = 1.0f;

			glColor4f(cr, cg, cb, fadeAlpha);
			glVertex3f(pos[i][0] + nx * w, pos[i][1] + ny * w, pos[i][2] + nz * w);
			glVertex3f(pos[i][0] - nx * w, pos[i][1] - ny * w, pos[i][2] - nz * w);
		}
		glEnd();
	}
}
// Apply one of the presets declared in ribbonsSettings.h.  Keeping the values
// there rather than here is what lets the tests check them against the ranges
// without a window or a registry.
static void applyPreset(const ribbons::Preset& p){
	dRibbonCount = p.ribbonCount;
	dRibbonLength = p.ribbonLength;
	dRibbonWidth = p.ribbonWidth;
	dSpeed = p.speed;
	dColorCycling = p.colorCycling;
	dTransparency = p.transparency;
	dFrameRateLimit = ribbons::kDefaultFrameRateLimit;
}


void setDefaults(int which){
	switch(which){
	case DEFAULTS1:  // Gentle
		applyPreset(ribbons::kGentle);
		break;
	case DEFAULTS2:  // Vivid
		applyPreset(ribbons::kVivid);
		break;
	case DEFAULTS3:  // Chaos
		applyPreset(ribbons::kChaos);
		break;
	}
}


void draw(){
	int i;

	// Clear to black every frame - ribbons are self-illuminated
	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0f, 0.0f, -20.0f);

	// Blending is switched on for the saver's lifetime in initSaver; a frame
	// only ever changes the blend function, which each ribbon pass sets itself.

	// Update and draw ribbons
	for(i = 0; i < dRibbonCount; i++){
		ribbonArray[i].update(frameTime);
		ribbonArray[i].draw();
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
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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


void reshape(int width, int height){
	glViewport(0, 0, width, height);
	aspectRatio = float(width) / float(height);

	// Narrower FOV for more elegant, less distorted perspective
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0, aspectRatio, 1.0, 10000.0);
}


#ifdef WIN32
void initSaver(HWND hwnd){
	RECT rect;

	// Window initialization
	hdc = GetDC(hwnd);
	setBestPixelFormat(hdc);
	hglrc = wglCreateContext(hdc);
	GetClientRect(hwnd, &rect);
	wglMakeCurrent(hdc, hglrc);

	reshape(rect.right, rect.bottom);
#endif
#ifdef RS_XSCREENSAVER
void initSaver(){
#endif

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	// Smooth edges for anti-aliased ribbon geometry
	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_POLYGON_SMOOTH);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	// No depth test - ribbons overlap visually by design
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);

	// Initialize ribbons
	ribbonArray = new Ribbon[dRibbonCount];

	// Initialize text
	textwriter = new rsText;

	readyToDraw = 1;
}


// Whatever cleanUp frees, cleanUp owns resetting: the saver is torn down and
// brought back up inside a single process by the test harness, and a stale
// pointer or a stale readyToDraw is what turns that into a crash.
void cleanUp(){
	// Free memory
	readyToDraw = 0;
	delete[] ribbonArray;
	ribbonArray = NULL;
	delete textwriter;
	textwriter = NULL;
}


#ifdef WIN32
void cleanUp(HWND hwnd){
	// Free memory
	readyToDraw = 0;
	delete[] ribbonArray;
	ribbonArray = NULL;
	delete textwriter;
	textwriter = NULL;

	// Kill device context
	ReleaseDC(hwnd, hdc);
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hglrc);
}
#endif


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


#ifdef WIN32
void readRegistry(){
	LONG result;
	HKEY skey;
	DWORD valtype, valsize, val;

	setDefaults(DEFAULTS1);

	result = RegOpenKeyEx(HKEY_CURRENT_USER, registryPath, 0, KEY_READ, &skey);
	if(result != ERROR_SUCCESS)
		return;

	valsize=sizeof(val);

	// Every value is clamped where it is read: what comes back is whatever is
	// in the registry, and the ribbon count and length size an allocation.
	// The frame rate limit belongs to the framework, so it is bounded with the
	// framework's own range rather than one restated here.
	result = RegQueryValueEx(skey, "RibbonCount", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dRibbonCount = ribbons::clampToRange(val, ribbons::kRibbonCount);
	result = RegQueryValueEx(skey, "RibbonLength", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dRibbonLength = ribbons::clampToRange(val, ribbons::kRibbonLength);
	result = RegQueryValueEx(skey, "RibbonWidth", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dRibbonWidth = ribbons::clampToRange(val, ribbons::kRibbonWidth);
	result = RegQueryValueEx(skey, "Speed", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dSpeed = ribbons::clampToRange(val, ribbons::kSpeed);
	result = RegQueryValueEx(skey, "ColorCycling", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dColorCycling = ribbons::clampToRange(val, ribbons::kColorCycling);
	result = RegQueryValueEx(skey, "Transparency", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dTransparency = ribbons::clampToRange(val, ribbons::kTransparency);
	result = RegQueryValueEx(skey, "FrameRateLimit", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dFrameRateLimit = rsWin32Saver::clampFrameRateLimit(val);

	RegCloseKey(skey);
}


void writeRegistry(){
    LONG result;
	HKEY skey;
	DWORD val, disp;

	result = RegCreateKeyEx(HKEY_CURRENT_USER, registryPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &skey, &disp);
	if(result != ERROR_SUCCESS)
		return;

	val = dRibbonCount;
	RegSetValueEx(skey, "RibbonCount", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dRibbonLength;
	RegSetValueEx(skey, "RibbonLength", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dRibbonWidth;
	RegSetValueEx(skey, "RibbonWidth", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dSpeed;
	RegSetValueEx(skey, "Speed", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dColorCycling;
	RegSetValueEx(skey, "ColorCycling", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dTransparency;
	RegSetValueEx(skey, "Transparency", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
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
			ShellExecute(NULL, "open", "https://www.reallyslick.com/", NULL, NULL, SW_SHOWNORMAL);
		}
	}
	return FALSE;
}


void initControls(HWND hdlg){
	char cval[16];

	SendDlgItemMessage(hdlg, RIBBONCOUNT, UDM_SETRANGE, 0,
		LPARAM(MAKELONG(DWORD(ribbons::kRibbonCount.hi), DWORD(ribbons::kRibbonCount.lo))));
	SendDlgItemMessage(hdlg, RIBBONCOUNT, UDM_SETPOS, 0, LPARAM(dRibbonCount));

	SendDlgItemMessage(hdlg, RIBBONLENGTH, TBM_SETRANGE, 0,
		LPARAM(MAKELONG(DWORD(ribbons::kRibbonLength.lo), DWORD(ribbons::kRibbonLength.hi))));
	SendDlgItemMessage(hdlg, RIBBONLENGTH, TBM_SETPOS, 1, LPARAM(dRibbonLength));
	SendDlgItemMessage(hdlg, RIBBONLENGTH, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, RIBBONLENGTH, TBM_SETPAGESIZE, 0, LPARAM(10));
	sprintf_s(cval, "%d", dRibbonLength);
	SendDlgItemMessage(hdlg, RIBBONLENGTHTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, RIBBONWIDTH, TBM_SETRANGE, 0,
		LPARAM(MAKELONG(DWORD(ribbons::kRibbonWidth.lo), DWORD(ribbons::kRibbonWidth.hi))));
	SendDlgItemMessage(hdlg, RIBBONWIDTH, TBM_SETPOS, 1, LPARAM(dRibbonWidth));
	SendDlgItemMessage(hdlg, RIBBONWIDTH, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, RIBBONWIDTH, TBM_SETPAGESIZE, 0, LPARAM(10));
	sprintf_s(cval, "%d", dRibbonWidth);
	SendDlgItemMessage(hdlg, RIBBONWIDTHTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, SPEED, TBM_SETRANGE, 0,
		LPARAM(MAKELONG(DWORD(ribbons::kSpeed.lo), DWORD(ribbons::kSpeed.hi))));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETPOS, 1, LPARAM(dSpeed));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, SPEED, TBM_SETPAGESIZE, 0, LPARAM(10));
	sprintf_s(cval, "%d", dSpeed);
	SendDlgItemMessage(hdlg, SPEEDTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, COLORCYCLING, TBM_SETRANGE, 0,
		LPARAM(MAKELONG(DWORD(ribbons::kColorCycling.lo), DWORD(ribbons::kColorCycling.hi))));
	SendDlgItemMessage(hdlg, COLORCYCLING, TBM_SETPOS, 1, LPARAM(dColorCycling));
	SendDlgItemMessage(hdlg, COLORCYCLING, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, COLORCYCLING, TBM_SETPAGESIZE, 0, LPARAM(10));
	sprintf_s(cval, "%d", dColorCycling);
	SendDlgItemMessage(hdlg, COLORCYCLINGTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, TRANSPARENCY, TBM_SETRANGE, 0,
		LPARAM(MAKELONG(DWORD(ribbons::kTransparency.lo), DWORD(ribbons::kTransparency.hi))));
	SendDlgItemMessage(hdlg, TRANSPARENCY, TBM_SETPOS, 1, LPARAM(dTransparency));
	SendDlgItemMessage(hdlg, TRANSPARENCY, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, TRANSPARENCY, TBM_SETPAGESIZE, 0, LPARAM(10));
	sprintf_s(cval, "%d", dTransparency);
	SendDlgItemMessage(hdlg, TRANSPARENCYTEXT, WM_SETTEXT, 0, LPARAM(cval));

	initFrameRateLimitSlider(hdlg, FRAMERATELIMIT, FRAMERATELIMITTEXT);
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
            dRibbonCount = SendDlgItemMessage(hdlg, RIBBONCOUNT, UDM_GETPOS, 0, 0);
			dRibbonLength = SendDlgItemMessage(hdlg, RIBBONLENGTH, TBM_GETPOS, 0, 0);
			dRibbonWidth = SendDlgItemMessage(hdlg, RIBBONWIDTH, TBM_GETPOS, 0, 0);
			dSpeed = SendDlgItemMessage(hdlg, SPEED, TBM_GETPOS, 0, 0);
			dColorCycling = SendDlgItemMessage(hdlg, COLORCYCLING, TBM_GETPOS, 0, 0);
			dTransparency = SendDlgItemMessage(hdlg, TRANSPARENCY, TBM_GETPOS, 0, 0);
			dFrameRateLimit = SendDlgItemMessage(hdlg, FRAMERATELIMIT, TBM_GETPOS, 0, 0);
			writeRegistry();
            // Fall through
        case IDCANCEL:
            EndDialog(hdlg, LOWORD(wpm));
            break;
		case DEFAULTS1:
			setDefaults(DEFAULTS1);
			initControls(hdlg);
			break;
		case DEFAULTS2:
			setDefaults(DEFAULTS2);
			initControls(hdlg);
			break;
		case DEFAULTS3:
			setDefaults(DEFAULTS3);
			initControls(hdlg);
			break;
		case ABOUT:
			DialogBox(mainInstance, MAKEINTRESOURCE(DLG_ABOUT), hdlg, aboutProc);
		}
        return TRUE;
	case WM_HSCROLL:
		if(HWND(lpm) == GetDlgItem(hdlg, RIBBONLENGTH)){
			ival = SendDlgItemMessage(hdlg, RIBBONLENGTH, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, RIBBONLENGTHTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, RIBBONWIDTH)){
			ival = SendDlgItemMessage(hdlg, RIBBONWIDTH, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, RIBBONWIDTHTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, SPEED)){
			ival = SendDlgItemMessage(hdlg, SPEED, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, SPEEDTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, COLORCYCLING)){
			ival = SendDlgItemMessage(hdlg, COLORCYCLING, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, COLORCYCLINGTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, TRANSPARENCY)){
			ival = SendDlgItemMessage(hdlg, TRANSPARENCY, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, TRANSPARENCYTEXT, WM_SETTEXT, 0, LPARAM(cval));
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
#endif


#ifdef RS_XSCREENSAVER
void handleArguments(int argc, char* argv[]){
	setDefaults(DEFAULTS1);
	getArgumentsValue(argc, argv, std::string("-ribboncount"), dRibbonCount,
		ribbons::kRibbonCount.lo, ribbons::kRibbonCount.hi);
	getArgumentsValue(argc, argv, std::string("-ribbonlength"), dRibbonLength,
		ribbons::kRibbonLength.lo, ribbons::kRibbonLength.hi);
	getArgumentsValue(argc, argv, std::string("-ribbonwidth"), dRibbonWidth,
		ribbons::kRibbonWidth.lo, ribbons::kRibbonWidth.hi);
	getArgumentsValue(argc, argv, std::string("-speed"), dSpeed,
		ribbons::kSpeed.lo, ribbons::kSpeed.hi);
	getArgumentsValue(argc, argv, std::string("-colorcycling"), dColorCycling,
		ribbons::kColorCycling.lo, ribbons::kColorCycling.hi);
	getArgumentsValue(argc, argv, std::string("-transparency"), dTransparency,
		ribbons::kTransparency.lo, ribbons::kTransparency.hi);
}
#endif
