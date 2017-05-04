/*
 * Copyright (C) 2010  Terence M. Welsh
 *
 * This file is part of Microcosm.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifdef WIN32
#include <windows.h>
#include <rsWin32Saver/rsWin32Saver.h>
#include <time.h>
#include <regstr.h>
#include <commctrl.h>
#include <process.h>
#include <resource.h>
#endif
#ifdef RS_XSCREENSAVER
#include <rsXScreenSaver/rsXScreenSaver.h>
#endif

//#include <iostream>
#include <vector>
#include <math.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include <rsText/rsText.h>
#include <Implicit/impCubeVolume.h>

#include "mirrorBox.h"
#include "rsCamera.h"
#include "brain.h"
#include "cube.h"
#include "cubesAndCapsules.h"
#include "cylinder.h"
#include "flower.h"
#include "grinder.h"
#include "knotAndSpheres.h"
#include "knotAndTorus.h"
#include "kube.h"
#include "kube2.h"
#include "kube3.h"
#include "kube4.h"
#include "metaballs.h"
#include "octahedron.h"
#include "orbit.h"
#include "ringOfTori.h"
#include "rings.h"
#include "spheresAndCapsules.h"
#include "stringOfEllipsoids.h"
#include "tennis.h"
#include "tetrahedron.h"
#include "torusBox.h"
#include "triangleOfSpheres.h"
#include "ufo.h"
#include "shaders.h"
#include "texture1d.h"


using namespace std;


// Globals
#ifdef WIN32
LPCTSTR registryPath = ("Software\\Really Slick\\Microcosm");
HDC hdc;
HGLRC hglrc;
#endif
int readyToDraw = 0;
float frameTime = 0.0f;
float computeTime = 0.0f;
float drawTime = 0.0f;
float aspectRatio;
float gHFov;
float gVFov;
rsCamera gCamera;
int gMode = 0;   // 0 = single centered Gizmo; 1 = kaleidoscope mode
float gModeTransition = 0.0f;
float gModeTransitionDir = 1.0f;
// text output
rsText* textwriter;
// Parameters edited in the dialog box
int dSingleTime;
int dKaleidoscopeTime;
int dBackground;
int dResolution;
int dDepth;
int dFov;
int dGizmoSpeed;
int dColorSpeed;
int dCameraSpeed;
int dShaders;
int dFog;

int gSpecificGizmo = -1;
float gNumberInputTimer = 0.0f;

MirrorBox mirrorbox;

vector<Gizmo*> gizmos;
unsigned int gGizmoIndex = 0;
ShapeVector shapes;
unsigned int gNumShapes = 0;
impCrawlPointVector crawlpoints;
impCubeVolume* volume0;
impCubeVolume* volume1;
impCubeVolume* volume2;
// Double buffers so that each of 3 volumes can store into one surface
// while the other is being used for drawing.
impSurface* volSurface0[2];
impSurface* volSurface1[2];
impSurface* volSurface2[2];
// Pointers to surfaces that can be used for drawing
impSurface* drawSurface0;
impSurface* drawSurface1;
impSurface* drawSurface2;

float gAmbient[4] = {0.2f, 0.2f, 0.2f, 0.0f};
float gDimAmbient[4];
float gNoAmbient[4] = {0.0f, 0.0f, 0.0f, 0.0f};
float gDiffuse0[4] = {1.0f, 1.0f, 1.0f, 0.0f};
float gSpecular0[4] = {1.0f, 1.0f, 1.0f, 0.0f};
float gDiffuse1[4] = {0.2f, 0.2f, 0.4f, 0.0f};
float gSpecular1[4] = {0.3f, 0.3f, 0.6f, 0.0f};
float gDimDiffuse0[4];
float gDimSpecular0[4];
float gDimDiffuse1[4];
float gDimSpecular1[4];

Texture1D* tex1d;

// eye coordinates for surface function
float sfEyeX, sfEyeY, sfEyeZ;

// easter egg
// Tennis gizmo is not immediately available
bool gTennisAvailable = false;

// multi-threading
// Two worker threads are used to compute frame (n+1)'s surfaces while frame n
// is being drawn.  This gives a bit of a performance advantage on multi-core
// processors.
// When the main thread is ready to draw on the screen, it signals the worker
// threads to start computing surfaces.  Then when the main thread is done
// drawing, it waits until the worker threads signal it that they are done.
// That way the main thread does not restart the draw() function and change
// the surface parameters until the worker threads are done using those parameters.
bool gUseThreads = true;
#ifdef WIN32
HANDLE gT0Start;  // for signaling thread 0 to start
HANDLE gT0End;  // for thread 0 to signal main thread that it is done
HANDLE gT1Start;
HANDLE gT1End;
#else
pthread_t gThread0;
pthread_t gThread1;
// conditional variables
pthread_cond_t gT0Start = PTHREAD_COND_INITIALIZER;  // for signaling thread 0 to start
pthread_cond_t gT0End = PTHREAD_COND_INITIALIZER;  // for thread 0 to signal main thread that it is done
pthread_cond_t gT1Start = PTHREAD_COND_INITIALIZER;
pthread_cond_t gT1End = PTHREAD_COND_INITIALIZER;
// each conditional variable requires a mutex
pthread_mutex_t gT0StartMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gT0EndMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gT1StartMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gT1EndMutex = PTHREAD_MUTEX_INITIALIZER;
#endif


//#include <fstream>
//std::ofstream outfile;


// function for mode 0: single gizmo
float surfaceFunction0(float* position){
	float value(0.0f);

	for(unsigned int i=0; i<gNumShapes; ++i)
		value += shapes[i]->value(position);

	return value;
}


// ... and with transition
float surfaceFunctionTransition0(float* position){
	float value(0.0f);

	for(unsigned int i=0; i<gNumShapes; ++i)
		value += shapes[i]->value(position);

	// transition
	float trans(((gModeTransition - 0.5f) * 1.5f + position[0]) * 10.0f);
	trans = trans * trans * trans;
	if(trans <= -50.0f)
		value = 0.0f;
	else
		value += (trans < 0.0f) ? trans : 0.0f;

	return value;
}


// function for mode 1: kaleidoscope
float surfaceFunction1(float* position){
	float value(0.0f);

	for(unsigned int i=0; i<gNumShapes; ++i)
		value += shapes[i]->value(position);

	// Lower the surface value near the viewpoint
	// to create a bubble around viewpoint.
	const float x(10.0f * (sfEyeX - position[0]));
	const float y(10.0f * (sfEyeY - position[1]));
	const float z(10.0f * (sfEyeZ - position[2]));
	const float hole((1.0f / (x * x + y * y + z * z)) - 1.0f);
	float hole_capped = hole;
	if(hole < 0.0f)
		hole_capped = 0.0f;
	value -= hole_capped * hole_capped;

	return value;
}


// ... and with transition
float surfaceFunctionTransition1(float* position){
	float value(0.0f);

	for(unsigned int i=0; i<gNumShapes; ++i)
		value += shapes[i]->value(position);

	// Lower the surface value near the viewpoint
	// to create a bubble around viewpoint.
	const float x(10.0f * (sfEyeX - position[0]));
	const float y(10.0f * (sfEyeY - position[1]));
	const float z(10.0f * (sfEyeZ - position[2]));
	const float hole((1.0f / (x * x + y * y + z * z)) - 1.0f);
	float hole_capped = hole;
	if(hole < 0.0f)
		hole_capped = 0.0f;
	value -= hole_capped * hole_capped;

	// transition
	float trans(((gModeTransition - 0.5f) * 1.5f + position[0]) * 10.0f);
	trans = trans * trans * trans;
	if(trans <= -50.0f)
		value = 0.0f;
	else
		value += (trans < 0.0f) ? trans : 0.0f;

	return value;
}


#ifdef WIN32
static unsigned __stdcall threadFunction0(void* arg){
	static rsTimer computeTimer;

	while(1){
		// Block until signal is received.  Mutex is unlocked while waiting.
		WaitForSingleObject(gT0Start, INFINITE);
		// Compute surface
		computeTimer.tick();
		volume0->makeSurface(crawlpoints);
		computeTime += float(computeTimer.tick());
		// Unlock mutex so draw() can start changing implicit surface definition again.
		SetEvent(gT0End);
	}
#else
void* threadFunction0(void* arg){
	static rsTimer computeTimer;

	// Conditional variables require their associated mutexes to start out locked
	pthread_mutex_lock(&gT0StartMutex);

	// Wait until main loop is signalable, then tell it that this thread has started
	pthread_mutex_lock(&gT0EndMutex);
	pthread_mutex_unlock(&gT0EndMutex);
	pthread_cond_signal(&gT0End);

	while(1){
		// Block until signal is received.  Mutex is unlocked while waiting.
		pthread_cond_wait(&gT0Start, &gT0StartMutex);
		// Compute surface
		computeTimer.tick();
		volume0->makeSurface(crawlpoints);
		computeTime += computeTimer.tick();
		// Wait until main loop is signalable, then tell it that this computation is complete.
		pthread_mutex_lock(&gT0EndMutex);
		pthread_mutex_unlock(&gT0EndMutex);
		pthread_cond_signal(&gT0End);
	}
#endif

	return 0;
}


#ifdef WIN32
static unsigned __stdcall threadFunction1(void* arg){
	static rsTimer computeTimer;

	while(1){
		// Block until signal is received.  Mutex is unlocked while waiting.
		WaitForSingleObject(gT1Start, INFINITE);
		// Compute surface
		computeTimer.tick();
		volume1->makeSurface(crawlpoints);
		volume2->makeSurface(crawlpoints);
		computeTime += float(computeTimer.tick());
		// Unlock mutex so draw() can start changing implicit surface definition again.
		SetEvent(gT1End);
	}
#else
void* threadFunction1(void* arg){
	static rsTimer computeTimer;

	// Conditional variables require their associated mutexes to start out locked
	pthread_mutex_lock(&gT1StartMutex);

	// Wait until main loop is signalable, then tell it that this thread has started
	pthread_mutex_lock(&gT1EndMutex);
	pthread_mutex_unlock(&gT1EndMutex);
	pthread_cond_signal(&gT1End);

	while(1){
		// Block until signal is received.  Mutex is unlocked while waiting.
		pthread_cond_wait(&gT1Start, &gT1StartMutex);
		// Compute surfaces
		computeTimer.tick();
		volume1->makeSurface(crawlpoints);
		volume2->makeSurface(crawlpoints);
		computeTime += computeTimer.tick();
		// Wait until main loop is signalable, then tell it that this computation is complete.
		pthread_mutex_lock(&gT1EndMutex);
		pthread_mutex_unlock(&gT1EndMutex);
		pthread_cond_signal(&gT1End);
	}
#endif

	return 0;
}


void chooseGizmo(int index = -1){
	if(index >= 0 && index < gizmos.size())  // choose specific Gizmo
		gGizmoIndex = index;
	else{
		// choose a new Gizmo at random, making sure to not choose the previous one
		unsigned int oldgizmo = gGizmoIndex;
		unsigned int range = gizmos.size();
		if(!gTennisAvailable)
			range --;
		while(gGizmoIndex == oldgizmo)
			gGizmoIndex = rsRandi(range);
	}

	// collect shapes
	shapes.clear();
	gizmos[gGizmoIndex]->getShapes(shapes);
	gNumShapes = shapes.size();
}


void draw(){
	static int first = 1;
	if(first){
		textwriter = new rsText;
		first = 0;
	}

	// count-down available time for pressing second digit
	if(gNumberInputTimer > 0.0f)
		gNumberInputTimer -= frameTime;

	// camera variables
	static float rotPhase[3] = {rsRandf(RS_PIx2), rsRandf(RS_PIx2), rsRandf(RS_PIx2)};
	static float rotRate[3] = {0.075f + rsRandf(0.05f), 0.075f + rsRandf(0.05f), 0.075f + rsRandf(0.05f)};
	static float rot[3];
	static rsVec cam0Start;
	static rsVec cam0End(rsRandf(2.0f) - 1.0f, rsRandf(2.0f) - 1.0f, -2.0f / tanf(0.5f * min(gHFov, gVFov)));
	static rsVec cam0Pos = cam0End;
	static rsMatrix cam0Background;
	static rsVec cam1Pos(0.1f, 0.2f, 0.3f);
	static float cam0t = 1.0f;

	// This is an Easter egg gizmo that shouldn't show up right when the saver
	// starts, so we'll make it available a bit later.
	static float easterEggTime = 0.0f;
	if(!gTennisAvailable){
		easterEggTime += frameTime;
		if(easterEggTime >= 1200.0f)
			gTennisAvailable = true;
	}

	// countdown to transition
	static float transitionTime = 0.0f;
	transitionTime += frameTime;
	const float ttime = (gMode == 0) ? float(dSingleTime) : float(dKaleidoscopeTime);
	if((gModeTransition > 1.0f && transitionTime > ttime) || gSpecificGizmo >= 0){
		if(gModeTransition > 1.0f)
			gModeTransition = 1.0f;
		gModeTransitionDir = -1.0f;
		transitionTime = 0.0f;
	}

	// transition
	if(gModeTransition <= 1.0f)
		gModeTransition += gModeTransitionDir * frameTime * 0.5f;
	if(gModeTransition < 0.0f){
		gModeTransitionDir = 1.0f;
		gModeTransition = 0.0f;
		// Switch modes if both are in use and we are not choosing a specific gizmo by keyboard press
		if(dSingleTime > 0 && dKaleidoscopeTime > 0 && gSpecificGizmo < 0){
			gMode = (gMode + 1) % 2;
			if(gMode == 0)
				chooseGizmo();
			else{
				if(rsRandi(4) == 0){
					rotPhase[0] = rotPhase[1] = rotPhase[2] = 0.0f;
					cam1Pos.set(0.0f, 0.0f, 0.0f);
				}
			}
		}
		else{
			if(gSpecificGizmo >= 0){
				chooseGizmo(gSpecificGizmo);
				gSpecificGizmo = -1;
			}
			else
				chooseGizmo();
		}
	}

	// camera movement
	for(int i=0; i<3; i++){
		rotPhase[i] += rotRate[i] * 0.1f * float(dCameraSpeed) * frameTime;
		if(rotPhase[i] >= RS_PIx2)
			rotPhase[i] -= RS_PIx2;
		rot[i] += sinf(rotPhase[i]) * 0.012f * float(dCameraSpeed) * frameTime;
	}

	rsMatrix rotMat, camMat;
	rotMat.makeRotate(rot[0], 1.0f, 0.0f, 0.0f);
	rotMat.rotate(rot[1], 0.0f, 1.0f, 0.0f);
	rotMat.rotate(rot[2], 0.0f, 0.0f, 1.0f);
	if(gMode == 0){
		cam0t += frameTime * 0.005f * float(dCameraSpeed);
		// chooose new point to move to
		float x, y, z;
		z = -0.6f / tanf(0.5f * min(gHFov, gVFov));
		if(cam0t >= 1.0f){
			cam0t -= 1.0f;
			cam0Start = cam0End;
			if(aspectRatio >= 1.0f){
				const float drift = aspectRatio - 0.5f;
				x = rsRandf(drift) - drift * 0.5f;
				y = rsRandf(0.4f) - 0.2f;
			}
			else{
				const float drift = (1.0f / aspectRatio) - 0.5f;
				x = rsRandf(0.4f) - 0.2f;
				y = rsRandf(drift) - drift * 0.5f;
			}
			cam0End.set(x, y, z);
		}
		// move camera
		float t = 0.5f * (1.0f - cosf(RS_PI * cam0t));
		cam0Pos = cam0Start * (1.0f - t) + cam0End * t;
		camMat.makeTranslate(cam0Pos);
		camMat.preMult(rotMat);
		gCamera.setViewMatrix(camMat);
		// background matrix
		rsMatrix backgroundrotmat;
		backgroundrotmat.makeRotate(2.0f, 1.0f, 1.0f, 1.0f);
		cam0Background.makeTranslate(0.0f, 0.0f, -4.0f);
		cam0Background.preMult(rotMat);
		cam0Background.preMult(backgroundrotmat);
	}
	else{
		cam1Pos[0] -= rotMat[2] * 0.06f * float(dCameraSpeed) * frameTime;
		cam1Pos[1] -= rotMat[6] * 0.06f * float(dCameraSpeed) * frameTime;
		cam1Pos[2] -= rotMat[10] * 0.06f * float(dCameraSpeed) * frameTime;
		if(cam1Pos[0] < -1.0f) cam1Pos[0] += 2.0f;
		if(cam1Pos[0] > 1.0f) cam1Pos[0] -= 2.0f;
		if(cam1Pos[1] < -1.0f) cam1Pos[1] += 2.0f;
		if(cam1Pos[1] > 1.0f) cam1Pos[1] -= 2.0f;
		if(cam1Pos[2] < -1.0f) cam1Pos[2] += 2.0f;
		if(cam1Pos[2] > 1.0f) cam1Pos[2] -= 2.0f;
		camMat.makeTranslate(-cam1Pos[0], -cam1Pos[1], -cam1Pos[2]);
		camMat.postMult(rotMat);
		gCamera.setViewMatrix(camMat);
	}

	// Breathing (suface value going up and down slightly)
	// Commented out because you really can't see the effect with everything else that's going on.
	// Also, it increases the chances of a surface getting accidentally clipped at the edge of an
	// impCubeVolume during single mode.  That would be ugly.
	/*static float breathe_phase = 0.0f;
	breathe_phase += frameTime;
	if(breathe_phase >= RS_PIx2)
		breathe_phase -= RS_PIx2;
	const float surface_value = 0.5f + 0.05f * cosf(breathe_phase);
	volume0->setSurfaceValue(surface_value);
	volume1->setSurfaceValue(surface_value);
	volume2->setSurfaceValue(surface_value);*/

	// store eye position values for surface function
	sfEyeX = fabsf(cam1Pos[0]) - 0.5f;
	sfEyeY = fabsf(cam1Pos[1]) - 0.5f;
	sfEyeZ = fabsf(cam1Pos[2]) - 0.5f;

	mirrorbox.update(frameTime);

	gizmos[gGizmoIndex]->update(frameTime);

	// collect crawl points
	crawlpoints.clear();
	gizmos[gGizmoIndex]->addCrawlPoints(crawlpoints);

	if(gMode == 0){
		if(gModeTransition < 1.0f)
			volume0->function = surfaceFunctionTransition0;
		else
			volume0->function = surfaceFunction0;
		// volume1 and volume2 are not used in mode 0
	}
	else{
		if(gModeTransition < 1.0f){
			volume0->function = surfaceFunctionTransition1;
			volume1->function = surfaceFunctionTransition1;
			volume2->function = surfaceFunctionTransition1;
		}
		else{
			volume0->function = surfaceFunction1;
			volume1->function = surfaceFunction1;
			volume2->function = surfaceFunction1;
		}
	}

	// Signal the worker threads to create implicit surfaces
	static int whichsurface = 0;
	if(gUseThreads){
		// Note that in this multithreaded mode, the comptute and draw happen simultaneously,
		// so what is being drawn by the main thread is always one frame behind the compute threads.
	
		// swap double-buffer impSurfaces
		drawSurface0 = volSurface0[whichsurface];
		drawSurface1 = volSurface1[whichsurface];
		drawSurface2 = volSurface2[whichsurface];
		whichsurface = (whichsurface + 1) % 2;
		volume0->setSurface(volSurface0[whichsurface]);
		volume1->setSurface(volSurface1[whichsurface]);
		volume2->setSurface(volSurface2[whichsurface]);

#ifdef WIN32
		// Block until thread0 is ready to be signaled again, then signal it.
		SetEvent(gT0Start);
		if(gMode == 1){  // only need low-LOD geometry for kaleidoscope mode
			// Block until thread1 is ready to be signaled again, then signal it.
			SetEvent(gT1Start);
		}
#else
		// Block until thread0 is ready to be signaled again, then signal it.
		pthread_mutex_lock(&gT0StartMutex);
		pthread_mutex_unlock(&gT0StartMutex);
		pthread_cond_signal(&gT0Start);
		if(gMode == 1){  // only need low-LOD geometry for kaleidoscope mode
			// Block until thread1 is ready to be signaled again, then signal it.
			pthread_mutex_lock(&gT1StartMutex);
			pthread_mutex_unlock(&gT1StartMutex);
			pthread_cond_signal(&gT1Start);
		}
#endif
	}
	else{
		static rsTimer computeTimer;
		computeTimer.tick();

		volume0->makeSurface(crawlpoints);
		// In kaleidoscope mode, also compute low-LOD surfaces.
		// Low-LOD surfaces take some load off the graphics card because it won't have to draw as many triangles.
		if(gMode == 1){
			volume1->makeSurface(crawlpoints);
			volume2->makeSurface(crawlpoints);
		}

		computeTime += float(computeTimer.tick());
	}

	static rsTimer drawTimer;
	drawTimer.tick();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	gCamera.apply();
	// must set light position immediately after view matrix is loaded
	float position0[4] = {1.0f, 1.0f, 1.0f, 0.0f};
	glLightfv(GL_LIGHT0, GL_POSITION, position0);
	float position1[4] = {-1.0f, -1.0f, 0.0f, 0.0f};
	glLightfv(GL_LIGHT1, GL_POSITION, position1);

	// animate texture
	static float tcphase[3] = {rsRandf(RS_PIx2), rsRandf(RS_PIx2), rsRandf(RS_PIx2)};
	tcphase[0] += frameTime * 0.0181f * float(dColorSpeed);  // oscillator for object space texcoord
	tcphase[1] += frameTime * 0.0221f * float(dColorSpeed);  // oscillator for eye space texcoord
	tcphase[2] += frameTime * 0.0037f * float(dColorSpeed);  // oscillator for moving between object and eye space texcoords
	for(int i=0; i<3; i++){
		if(tcphase[i] >= RS_PIx2)
			tcphase[i] -= RS_PIx2;
	}
	float tcmix[] = {sinf(tcphase[0]) * 0.5f + 0.5f, sinf(tcphase[1]) * 0.6f + 0.5f, sinf(tcphase[2]) + 0.5f};
	for(int i=0; i<3; i++){
		if(tcmix[i] < 0.0f)
			tcmix[i] = 0.0f;
		if(tcmix[i] > 1.0f)
			tcmix[i] = 1.0f;
	}
	if(dShaders){  // activate and update shader
		glUseProgramObjectARB(shaderProgram);
		glUniform3fARB(texCoordMixUniform, tcmix[0], tcmix[1], tcmix[2]);
	}
	else{  // or fallback to non-shader rendering
		// shift texture coords around
		float xPlane[] = {1.0f, 1.0f, 1.0f, tcmix[0]};
		glTexGenfv(GL_S, GL_OBJECT_PLANE, xPlane);
		glEnable(GL_TEXTURE_GEN_S);
	}
	glEnable(GL_TEXTURE_1D);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	tex1d->update();

	if(gMode == 0){
		if(dBackground){
			// render gizmo as background
			glLightfv(GL_LIGHT0, GL_AMBIENT, gDimAmbient);
			glLightfv(GL_LIGHT0, GL_DIFFUSE, gDimDiffuse0);
			glLightfv(GL_LIGHT0, GL_SPECULAR, gDimSpecular0);
			glLightfv(GL_LIGHT1, GL_AMBIENT, gNoAmbient);
			glLightfv(GL_LIGHT1, GL_DIFFUSE, gDimDiffuse1);
			glLightfv(GL_LIGHT1, GL_SPECULAR, gDimSpecular1);

			if(dFog){
				glEnable(GL_FOG);
				glFogf(GL_FOG_START, 0.0f);
				glFogf(GL_FOG_END, 20.0f);
			}

			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
				glLoadIdentity();
				float fov = 10.0f;
				if(aspectRatio > 1.0f)
					fov /= aspectRatio;
				gluPerspective(fov, aspectRatio, 0.01f, 50.0f);
				glMatrixMode(GL_MODELVIEW);
				glPushMatrix();
					glLoadMatrixf(cam0Background.get());
					drawSurface0->draw();
				glPopMatrix();
				glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
			glClear(GL_DEPTH_BUFFER_BIT);
		}

		// render gizmo normally
		glLightfv(GL_LIGHT0, GL_AMBIENT, gAmbient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, gDiffuse0);
		glLightfv(GL_LIGHT0, GL_SPECULAR, gSpecular0);
		glLightfv(GL_LIGHT1, GL_AMBIENT, gNoAmbient);
		glLightfv(GL_LIGHT1, GL_DIFFUSE, gDiffuse1);
		glLightfv(GL_LIGHT1, GL_SPECULAR, gSpecular1);

		drawSurface0->draw();
	}
	else{
#if USE_CLIP_PLANES
		glEnable(GL_CLIP_PLANE0);
		glEnable(GL_CLIP_PLANE1);
		glEnable(GL_CLIP_PLANE2);
		glEnable(GL_CLIP_PLANE3);
		glEnable(GL_CLIP_PLANE4);
		glEnable(GL_CLIP_PLANE5);
#endif

		if(dFog){
			glEnable(GL_FOG);
			glFogf(GL_FOG_START, float(dDepth) * 0.667f);
			glFogf(GL_FOG_END, float(dDepth) * 2.0f);
		}

		glLightfv(GL_LIGHT0, GL_AMBIENT, gAmbient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, gDiffuse0);
		glLightfv(GL_LIGHT0, GL_SPECULAR, gSpecular0);
		glLightfv(GL_LIGHT1, GL_AMBIENT, gNoAmbient);
		glLightfv(GL_LIGHT1, GL_DIFFUSE, gDiffuse1);
		glLightfv(GL_LIGHT1, GL_SPECULAR, gSpecular1);

		for(int k=-dDepth; k<=dDepth; k++){
			for(int j=-dDepth; j<=dDepth; j++){
				for(int i=-dDepth; i<=dDepth; i++){
					const float x(float(2 * i));
					const float y(float(2 * j));
					const float z(float(2 * k));
					if(gCamera.inViewVolume(rsVec(x, y, z), 1.7320508f)){
						const float eye_x(x - cam1Pos[0]);
						const float eye_y(y - cam1Pos[1]);
						const float eye_z(z - cam1Pos[2]);
						mirrorbox.draw(x, y, z, eye_x, eye_y, eye_z);
					}
				}
			}
		}

#if USE_CLIP_PLANES
		glDisable(GL_CLIP_PLANE0);
		glDisable(GL_CLIP_PLANE1);
		glDisable(GL_CLIP_PLANE2);
		glDisable(GL_CLIP_PLANE3);
		glDisable(GL_CLIP_PLANE4);
		glDisable(GL_CLIP_PLANE5);
#endif
	}

	if(dShaders)
		glUseProgramObjectARB(0);
	else
		glDisable(GL_TEXTURE_GEN_S);

	glDisable(GL_FOG);

	gCamera.revoke();

	drawTime += float(drawTimer.tick());

	// Pause here until worker threads are finished.  This prevents draw() from starting over
	// and changing the surface parameters until the worker threads are done using them.
	if(gUseThreads){
#ifdef WIN32
		if(gMode == 1)
			WaitForSingleObject(gT1End, INFINITE);
		WaitForSingleObject(gT0End, INFINITE);
#else
		// Block until signal is received.  Mutex is unlocked while waiting.
		if(gMode == 1)
			pthread_cond_wait(&gT1End, &gT1EndMutex);
		pthread_cond_wait(&gT0End, &gT0EndMutex);
#endif
	}

	// print text
	static float totalTime = 0.0f;
	totalTime += frameTime;
	static vector<string> strvec;
	static int frames = 0;
	++frames;
	const int frame_period = 60;
	if(frames >= frame_period){
		strvec.clear();
		std::string str1 = "         FPS = " + to_string(float(frame_period) / totalTime);
		strvec.push_back(str1);
		std::string str2 = "compute time = " + to_string(computeTime / float(frame_period) );
		strvec.push_back(str2);
		std::string str3 = "   draw time = " + to_string(drawTime / float(frame_period) );
		strvec.push_back(str3);
		totalTime = 0.0f;
		computeTime = 0.0f;
		drawTime = 0.0f;
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
		textwriter->draw(strvec);

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

	if(readyToDraw && !isSuspended && !checkingPassword)
		draw();
}


void setDefaults(int which){
	switch(which){
	case 1:  // Simple
		dSingleTime = 60;
		dKaleidoscopeTime = 0;
		dBackground = 0;
		dResolution = 50;
		dDepth = 4;
		dFov = 60;
		dGizmoSpeed = 10;
		dColorSpeed = 10;
		dCameraSpeed = 10;
		dShaders = 1;
		dFog = 1;
		break;
	case 2:  // Hyperspace
		dSingleTime = 0;
		dKaleidoscopeTime = 120;
		dBackground = 25;
		dResolution = 40;
		dDepth = 5;
		dFov = 100;
		dGizmoSpeed = 20;
		dColorSpeed = 20;
		dCameraSpeed = 50;
		dShaders = 1;
		dFog = 1;
		break;
	case 0:  // Regular
	default:
		dSingleTime = 60;
		dKaleidoscopeTime = 60;
		dBackground = 25;
		dResolution = 50;
		dDepth = 4;
		dFov = 60;
		dGizmoSpeed = 10;
		dColorSpeed = 10;
		dCameraSpeed = 10;
		dShaders = 1;
		dFog = 1;
		// This is the sort of thing somebody sets once and never changes.  It's more of
		// a technical choice than a visual choice like the rest of the defaults.
		//dFrameRateLimit = 0;
		break;
	}
}


#ifdef RS_XSCREENSAVER
void handleCommandLine(int argc, char* argv[]){
	setDefaults(0);
	getArgumentsValue(argc, argv, std::string("-stime"), dSingleTime, 0, 300);
	getArgumentsValue(argc, argv, std::string("-ktime"), dKaleidoscopeTime, 0, 300);
	getArgumentsValue(argc, argv, std::string("-background"), dBackground, 0, 100);
	getArgumentsValue(argc, argv, std::string("-resolution"), dResolution, 20, 100);
	getArgumentsValue(argc, argv, std::string("-depth"), dDepth, 1, 10);
	getArgumentsValue(argc, argv, std::string("-gizmospeed"), dGizmoSpeed, 1, 100);
	getArgumentsValue(argc, argv, std::string("-colorspeed"), dColorSpeed, 1, 100);
	getArgumentsValue(argc, argv, std::string("-cameraspeed"), dCameraSpeed, 1, 100);
	getArgumentsValue(argc, argv, std::string("-shaders"), dShaders, 0, 1);
	getArgumentsValue(argc, argv, std::string("-fog"), dFog, 0, 1);
}
#endif


void reshape(int width, int height){
	glViewport(0, 0, width, height);
	aspectRatio = float(width) / float(height);

	// calculate fov and set up projection matrix
	const float fov = dFov * RS_DEG2RAD;
	if(aspectRatio > 1.0f){
		gHFov = fov;
		gVFov = 2.0f * atanf(tanf(fov * 0.5f) / aspectRatio);
	}
	else{
		gHFov = 2.0f * atanf(tanf(fov * 0.5f) * aspectRatio);
		gVFov = fov;
	}
	gCamera.setProjectionMatrix(gVFov, aspectRatio, 0.01f, 50.0f);
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

	// initialize extensions
	if(!initExtensions())
		dShaders = 0;

	reshape(rect.right, rect.bottom);
#endif
#ifdef RS_XSCREENSAVER
void initSaver(){
#endif
	srand((unsigned)time(NULL));

	// Limit memory consumption because the Windows previewer is just too darn slow
	if(doingPreview){
		dResolution = 20;
		if(dDepth > 2)
			dDepth = 2;
	};

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnable(GL_DEPTH_TEST);
	glFrontFace(GL_CCW);
	glEnable(GL_CULL_FACE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

	// pre-multiplied alpha.  Diffuse color must be multiplied by diffuse alpha in shader
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	//glEnable(GL_BLEND);

	glEnable(GL_NORMALIZE);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);
	//glEnable(GL_COLOR_MATERIAL);
	glMaterialfv(GL_FRONT, GL_SPECULAR, gSpecular0);
	glMaterialf(GL_FRONT, GL_SHININESS, 30.0f);
	//glColorMaterial(GL_FRONT, GL_SPECULAR);
	//glColor3f(0.7f, 0.7f, 0.7f);
	//glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

	for(int i=0; i<4; i++){
		const float mult = (i==3) ? 1.0f : float(dBackground) * 0.01f;
		gDimAmbient[i] = gAmbient[i] * mult;
		gDimDiffuse0[i] = gDiffuse0[i] * mult;
		gDimSpecular0[i] = gSpecular0[i] * mult;
		gDimDiffuse1[i] = gDiffuse1[i] * mult;
		gDimSpecular1[i] = gSpecular1[i] * mult;
	}

	// initialize gizmos	gizmos.clear();
	{ Metaballs* gizmo = new Metaballs(7);  gizmos.push_back(gizmo); }
	{ TriangleOfSpheres* gizmo = new TriangleOfSpheres(5);  gizmos.push_back(gizmo); }
	{ TriangleOfSpheres* gizmo = new TriangleOfSpheres(7);  gizmos.push_back(gizmo); }
	{ TriangleOfSpheres* gizmo = new TriangleOfSpheres(8);  gizmos.push_back(gizmo); }
	{ StringOfEllipsoids* gizmo = new StringOfEllipsoids(5, 0.7f);  gizmos.push_back(gizmo); }
	{ StringOfEllipsoids* gizmo = new StringOfEllipsoids(6, 0.7f);  gizmos.push_back(gizmo); }
	{ StringOfEllipsoids* gizmo = new StringOfEllipsoids(7, 1.0f);  gizmos.push_back(gizmo); }
	{ StringOfEllipsoids* gizmo = new StringOfEllipsoids(8, 0.7f);  gizmos.push_back(gizmo); }
	{ StringOfEllipsoids* gizmo = new StringOfEllipsoids(8, 1.2f);  gizmos.push_back(gizmo); }
	{ StringOfEllipsoids* gizmo = new StringOfEllipsoids(10, 0.9f);  gizmos.push_back(gizmo); }
	{ StringOfEllipsoids* gizmo = new StringOfEllipsoids(10, 2.0f);  gizmos.push_back(gizmo); }
	{ Brain* gizmo = new Brain(3);  gizmos.push_back(gizmo); }
	{ Brain* gizmo = new Brain(4);  gizmos.push_back(gizmo); }
	{ Brain* gizmo = new Brain(5);  gizmos.push_back(gizmo); }
	{ Brain* gizmo = new Brain(6);  gizmos.push_back(gizmo); }
	{ Flower* gizmo = new Flower(10, 0.2f);  gizmos.push_back(gizmo); }
	{ Flower* gizmo = new Flower(14, 0.2f);  gizmos.push_back(gizmo); }
	{ Flower* gizmo = new Flower(14, 1.0f);  gizmos.push_back(gizmo); }
	{ Flower* gizmo = new Flower(18, 0.2f);  gizmos.push_back(gizmo); }
	{ KnotAndSpheres* gizmo = new KnotAndSpheres(2, 3, 3);  gizmos.push_back(gizmo); }
	{ KnotAndSpheres* gizmo = new KnotAndSpheres(2, 5, 3);  gizmos.push_back(gizmo); }
	{ KnotAndSpheres* gizmo = new KnotAndSpheres(3, 2, 2);  gizmos.push_back(gizmo); }
	{ KnotAndSpheres* gizmo = new KnotAndSpheres(3, 4, 2);  gizmos.push_back(gizmo); }
	{ KnotAndSpheres* gizmo = new KnotAndSpheres(3, 5, 2);  gizmos.push_back(gizmo); }
	{ KnotAndTorus* gizmo = new KnotAndTorus(2, 3);  gizmos.push_back(gizmo); }
	{ KnotAndTorus* gizmo = new KnotAndTorus(3, 2);  gizmos.push_back(gizmo); }
	{ KnotAndTorus* gizmo = new KnotAndTorus(3, 4);  gizmos.push_back(gizmo); }
	{ Orbit* gizmo = new Orbit;  gizmos.push_back(gizmo); }
	{ TorusBox* gizmo = new TorusBox;  gizmos.push_back(gizmo); }
	{ RingOfTori* gizmo = new RingOfTori(3);  gizmos.push_back(gizmo); }
	{ RingOfTori* gizmo = new RingOfTori(4);  gizmos.push_back(gizmo); }
	{ RingOfTori* gizmo = new RingOfTori(5);  gizmos.push_back(gizmo); }
	{ SpheresAndCapsules* gizmo = new SpheresAndCapsules(4);  gizmos.push_back(gizmo); }
	{ SpheresAndCapsules* gizmo = new SpheresAndCapsules(5);  gizmos.push_back(gizmo); }
	{ SpheresAndCapsules* gizmo = new SpheresAndCapsules(6);  gizmos.push_back(gizmo); }
	{ SpheresAndCapsules* gizmo = new SpheresAndCapsules(7);  gizmos.push_back(gizmo); }
	{ CubesAndCapsules* gizmo = new CubesAndCapsules(6, 0.6f);  gizmos.push_back(gizmo); }
	{ CubesAndCapsules* gizmo = new CubesAndCapsules(6, 1.0472f);  gizmos.push_back(gizmo); }
	{ UFO* gizmo = new UFO(7);  gizmos.push_back(gizmo); }
	{ UFO* gizmo = new UFO(10);  gizmos.push_back(gizmo); }
	{ Rings* gizmo = new Rings(5);  gizmos.push_back(gizmo); }
	{ Rings* gizmo = new Rings(6);  gizmos.push_back(gizmo); }
	{ Rings* gizmo = new Rings(7);  gizmos.push_back(gizmo); }
	{ Grinder* gizmo = new Grinder(4);  gizmos.push_back(gizmo); }
	{ Grinder* gizmo = new Grinder(5);  gizmos.push_back(gizmo); }
	{ Grinder* gizmo = new Grinder(6);  gizmos.push_back(gizmo); }  // slowest
	{ Kube* gizmo = new Kube;  gizmos.push_back(gizmo); }
	{ Kube2* gizmo = new Kube2;  gizmos.push_back(gizmo); }
	{ Kube3* gizmo = new Kube3;  gizmos.push_back(gizmo); }
	{ Kube4* gizmo = new Kube4;  gizmos.push_back(gizmo); }
	{ Cube* gizmo = new Cube;  gizmos.push_back(gizmo); }
	{ Cylinder* gizmo = new Cylinder();  gizmos.push_back(gizmo); }
	{ Octahedron* gizmo = new Octahedron;  gizmos.push_back(gizmo); }  // 2nd slowest
	{ Tetrahedron* gizmo = new Tetrahedron;  gizmos.push_back(gizmo); }
	{ Tennis* gizmo = new Tennis;  gizmos.push_back(gizmo); }

	chooseGizmo();

	// initialize surfaces
	volSurface0[0] = new impSurface;
	volSurface0[1] = new impSurface;
	volSurface1[0] = new impSurface;
	volSurface1[1] = new impSurface;
	volSurface2[0] = new impSurface;
	volSurface2[1] = new impSurface;
	// Pointers to surfaces that can be used for drawing
	drawSurface0 = volSurface0[0];
	drawSurface1 = volSurface1[0];
	drawSurface2 = volSurface2[0];

	// initialize volumes
	volume0 = new impCubeVolume;
	volume0->init(dResolution, dResolution, dResolution, 1.0f / float(dResolution));
	volume0->useFastNormals(false);
	volume0->setCrawlFromSides(true);
	volume0->setSurface(volSurface0[0]);

	int v1res = dResolution * 2 / 3;
	if(v1res < 18)
		v1res = 18;
	volume1 = new impCubeVolume;
	volume1->init(v1res, v1res, v1res, 1.0f / float(v1res));
	volume1->useFastNormals(false);
	volume1->setCrawlFromSides(true);
	volume1->setSurface(volSurface1[0]);

	int v2res = dResolution / 3;
	if(v2res < 16)
		v2res = 16;
	volume2 = new impCubeVolume;
	volume2->init(v2res, v2res, v2res, 1.0f / float(v2res));
	volume2->useFastNormals(false);
	volume2->setCrawlFromSides(true);
	volume2->setSurface(volSurface2[0]);

	tex1d = new Texture1D;

	if(dShaders)
		initShaders();
	else{
		glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
	}

	// which mode to start in
	if(dKaleidoscopeTime > 0){
		if(dSingleTime > 0)
			gMode = rsRandi(2);
		else
			gMode = 1;
	}

	// setup fog
	float fog_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	glFogfv(GL_FOG_COLOR, fog_color);
	glFogf(GL_FOG_MODE, GL_LINEAR);
	// Fog will appear in the shader with or without GL_FOG enabled.
	// This effectively disables fog for the shader.
	glFogf(GL_FOG_START, 100.0f);
	glFogf(GL_FOG_END, 1000.0f);

	// test code for starting up in a specific mode with a specific gizmo
	//gMode = 0;
	//chooseGizmo(0);

	//outfile.open("outfile");

	// threading
	if(gUseThreads){
#ifdef WIN32
		gT0Start = CreateEvent(NULL, FALSE, FALSE, NULL);
		gT1Start = CreateEvent(NULL, FALSE, FALSE, NULL);
		gT0End = CreateEvent(NULL, FALSE, FALSE, NULL);
		gT1End = CreateEvent(NULL, FALSE, FALSE, NULL);
		// Create threads
		unsigned int junk;
		HANDLE thread0 = (HANDLE)_beginthreadex(NULL, 0, threadFunction0, NULL, 0, &junk);
		HANDLE thread1 = (HANDLE)_beginthreadex(NULL, 0, threadFunction1, NULL, 0, &junk);
		if(thread0 == NULL || thread1 == NULL)
			gUseThreads = false;
#else
		// Conditional variables require their associated mutexes to start out locked
		pthread_mutex_lock(&gT0EndMutex);
		pthread_mutex_lock(&gT1EndMutex);

		// Create threads
		if(0 != pthread_create(&gThread0, NULL, threadFunction0, NULL)
			|| 0 != pthread_create(&gThread1, NULL, threadFunction1, NULL))
			gUseThreads = false;

		// Block until signal is received.  Mutex is unlocked while waiting.
		// This tells us that the threads have started successfully and that they have locked
		// the "start" mutexes.  If we don't wait here, the threads might not lock the "start"
		// mutexes in time and we'll get a deadlock in draw().
		pthread_cond_wait(&gT0End, &gT0EndMutex);
		pthread_cond_wait(&gT1End, &gT1EndMutex);
#endif
	}

	readyToDraw = 1;
}


#ifdef RS_XSCREENSAVER
void cleanUp(){
	// Free memory
}
#endif


#ifdef WIN32
void cleanUp(HWND hwnd){
	// Free memory

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

	setDefaults(0);

	result = RegOpenKeyEx(HKEY_CURRENT_USER, registryPath, 0, KEY_READ, &skey);
	if(result != ERROR_SUCCESS)
		return;

	valsize=sizeof(val);

	result = RegQueryValueEx(skey, "KaleidoscopeTime", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dKaleidoscopeTime = val;
	result = RegQueryValueEx(skey, "SingleTime", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dSingleTime = val;
	result = RegQueryValueEx(skey, "SingleBackground", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dBackground = val;
	result = RegQueryValueEx(skey, "Resolution", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dResolution = val;
	result = RegQueryValueEx(skey, "Depth", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dDepth = val;
	result = RegQueryValueEx(skey, "Fov", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dFov = val;
	result = RegQueryValueEx(skey, "CameraSpeed", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dCameraSpeed = val;
	result = RegQueryValueEx(skey, "GizmoSpeed", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dGizmoSpeed = val;
	result = RegQueryValueEx(skey, "ColorSpeed", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dColorSpeed = val;
	result = RegQueryValueEx(skey, "Shaders", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dShaders = val;
	result = RegQueryValueEx(skey, "Fog", 0, &valtype, (LPBYTE)&val, &valsize);
	if(result == ERROR_SUCCESS)
		dFog = val;
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

	val = dKaleidoscopeTime;
	RegSetValueEx(skey, "KaleidoscopeTime", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dSingleTime;
	RegSetValueEx(skey, "SingleTime", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dBackground;
	RegSetValueEx(skey, "SingleBackground", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dResolution;
	RegSetValueEx(skey, "Resolution", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dDepth;
	RegSetValueEx(skey, "Depth", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dFov;
	RegSetValueEx(skey, "Fov", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dGizmoSpeed;
	RegSetValueEx(skey, "GizmoSpeed", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dColorSpeed;
	RegSetValueEx(skey, "ColorSpeed", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dCameraSpeed;
	RegSetValueEx(skey, "CameraSpeed", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dShaders;
	RegSetValueEx(skey, "Shaders", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dFog;
	RegSetValueEx(skey, "Fog", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));
	val = dFrameRateLimit;
	RegSetValueEx(skey, "FrameRateLimit", 0, REG_DWORD, (CONST BYTE*)&val, sizeof(val));

	RegCloseKey(skey);
}


BOOL CALLBACK aboutProc(HWND hdlg, UINT msg, WPARAM wpm, LPARAM lpm){
	switch(msg){
	case WM_CTLCOLORSTATIC:
		if((HWND(lpm) == GetDlgItem(hdlg, WEBPAGE))){
			SetTextColor(HDC(wpm), RGB(0,0,255));
			SetBkColor(HDC(wpm), COLORREF(GetSysColor(COLOR_3DFACE)));
			return int(GetSysColorBrush(COLOR_3DFACE));
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
			break;
		}
	}
	return FALSE;
}


void initControls(HWND hdlg){
	char cval[16];

	SendDlgItemMessage(hdlg, KALEIDOSCOPETIME, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(0), DWORD(300))));
	SendDlgItemMessage(hdlg, KALEIDOSCOPETIME, TBM_SETPOS, 1, LPARAM(dKaleidoscopeTime));
	SendDlgItemMessage(hdlg, KALEIDOSCOPETIME, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, KALEIDOSCOPETIME, TBM_SETPAGESIZE, 0, LPARAM(10));
	sprintf_s(cval, "%d seconds", dKaleidoscopeTime);
	SendDlgItemMessage(hdlg, KALEIDOSCOPETIMETEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, SINGLETIME, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(0), DWORD(300))));
	SendDlgItemMessage(hdlg, SINGLETIME, TBM_SETPOS, 1, LPARAM(dSingleTime));
	SendDlgItemMessage(hdlg, SINGLETIME, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, SINGLETIME, TBM_SETPAGESIZE, 0, LPARAM(10));
	sprintf_s(cval, "%d seconds", dSingleTime);
	SendDlgItemMessage(hdlg, SINGLETIMETEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, SINGLEBACKGROUND, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(0), DWORD(100))));
	SendDlgItemMessage(hdlg, SINGLEBACKGROUND, TBM_SETPOS, 1, LPARAM(dBackground));
	SendDlgItemMessage(hdlg, SINGLEBACKGROUND, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, SINGLEBACKGROUND, TBM_SETPAGESIZE, 0, LPARAM(10));
	sprintf_s(cval, "%d %", dBackground);
	SendDlgItemMessage(hdlg, SINGLEBACKGROUNDTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, RESOLUTION, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(20), DWORD(100))));
	SendDlgItemMessage(hdlg, RESOLUTION, TBM_SETPOS, 1, LPARAM(dResolution));
	SendDlgItemMessage(hdlg, RESOLUTION, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, RESOLUTION, TBM_SETPAGESIZE, 0, LPARAM(5));
	sprintf_s(cval, "%d", dResolution);
	SendDlgItemMessage(hdlg, RESOLUTIONTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, DEPTH, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(1), DWORD(10))));
	SendDlgItemMessage(hdlg, DEPTH, TBM_SETPOS, 1, LPARAM(dDepth));
	SendDlgItemMessage(hdlg, DEPTH, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, DEPTH, TBM_SETPAGESIZE, 0, LPARAM(1));
	sprintf_s(cval, "%d", dDepth);
	SendDlgItemMessage(hdlg, DEPTHTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, FOV, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(10), DWORD(150))));
	SendDlgItemMessage(hdlg, FOV, TBM_SETPOS, 1, LPARAM(dFov));
	SendDlgItemMessage(hdlg, FOV, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, FOV, TBM_SETPAGESIZE, 0, LPARAM(10));
	sprintf_s(cval, "%d", dFov);
	SendDlgItemMessage(hdlg, FOVTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, GIZMOSPEED, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(1), DWORD(100))));
	SendDlgItemMessage(hdlg, GIZMOSPEED, TBM_SETPOS, 1, LPARAM(dGizmoSpeed));
	SendDlgItemMessage(hdlg, GIZMOSPEED, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, GIZMOSPEED, TBM_SETPAGESIZE, 0, LPARAM(5));
	sprintf_s(cval, "%d", dGizmoSpeed);
	SendDlgItemMessage(hdlg, GIZMOSPEEDTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, COLORSPEED, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(0), DWORD(100))));
	SendDlgItemMessage(hdlg, COLORSPEED, TBM_SETPOS, 1, LPARAM(dColorSpeed));
	SendDlgItemMessage(hdlg, COLORSPEED, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, COLORSPEED, TBM_SETPAGESIZE, 0, LPARAM(5));
	sprintf_s(cval, "%d", dColorSpeed);
	SendDlgItemMessage(hdlg, COLORSPEEDTEXT, WM_SETTEXT, 0, LPARAM(cval));

	SendDlgItemMessage(hdlg, CAMERASPEED, TBM_SETRANGE, 0, LPARAM(MAKELONG(DWORD(1), DWORD(100))));
	SendDlgItemMessage(hdlg, CAMERASPEED, TBM_SETPOS, 1, LPARAM(dCameraSpeed));
	SendDlgItemMessage(hdlg, CAMERASPEED, TBM_SETLINESIZE, 0, LPARAM(1));
	SendDlgItemMessage(hdlg, CAMERASPEED, TBM_SETPAGESIZE, 0, LPARAM(5));
	sprintf_s(cval, "%d", dCameraSpeed);
	SendDlgItemMessage(hdlg, CAMERASPEEDTEXT, WM_SETTEXT, 0, LPARAM(cval));

	CheckDlgButton(hdlg, SHADERS, dShaders);

	CheckDlgButton(hdlg, FOG, dFog);

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
			dKaleidoscopeTime = SendDlgItemMessage(hdlg, KALEIDOSCOPETIME, TBM_GETPOS, 0, 0);
			dSingleTime = SendDlgItemMessage(hdlg, SINGLETIME, TBM_GETPOS, 0, 0);
			dBackground = SendDlgItemMessage(hdlg, SINGLEBACKGROUND, TBM_GETPOS, 0, 0);
			dDepth = SendDlgItemMessage(hdlg, DEPTH, TBM_GETPOS, 0, 0);
			dFov = SendDlgItemMessage(hdlg, FOV, TBM_GETPOS, 0, 0);
			dResolution = SendDlgItemMessage(hdlg, RESOLUTION, TBM_GETPOS, 0, 0);
			dGizmoSpeed = SendDlgItemMessage(hdlg, GIZMOSPEED, TBM_GETPOS, 0, 0);
			dColorSpeed = SendDlgItemMessage(hdlg, COLORSPEED, TBM_GETPOS, 0, 0);
			dCameraSpeed = SendDlgItemMessage(hdlg, CAMERASPEED, TBM_GETPOS, 0, 0);
			dShaders = (IsDlgButtonChecked(hdlg, SHADERS) == BST_CHECKED);
			dFog = (IsDlgButtonChecked(hdlg, FOG) == BST_CHECKED);
			dFrameRateLimit = SendDlgItemMessage(hdlg, FRAMERATELIMIT, TBM_GETPOS, 0, 0);
			writeRegistry();
			// Fall through
		case IDCANCEL:
			EndDialog(hdlg, LOWORD(wpm));
			break;
		case DEFAULTS0:
			setDefaults(0);
			initControls(hdlg);
			break;
		case DEFAULTS1:
			setDefaults(1);
			initControls(hdlg);
			break;
		case DEFAULTS2:
			setDefaults(2);
			initControls(hdlg);
			break;
		case ABOUT:
			DialogBox(mainInstance, MAKEINTRESOURCE(DLG_ABOUT), hdlg, DLGPROC(aboutProc));
			break;
		}
		return TRUE;
	case WM_HSCROLL:
		if(HWND(lpm) == GetDlgItem(hdlg, KALEIDOSCOPETIME)){
			ival = SendDlgItemMessage(hdlg, KALEIDOSCOPETIME, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d seconds", ival);
			SendDlgItemMessage(hdlg, KALEIDOSCOPETIMETEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, SINGLETIME)){
			ival = SendDlgItemMessage(hdlg, SINGLETIME, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d seconds", ival);
			SendDlgItemMessage(hdlg, SINGLETIMETEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, SINGLEBACKGROUND)){
			ival = SendDlgItemMessage(hdlg, SINGLEBACKGROUND, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d %", ival);
			SendDlgItemMessage(hdlg, SINGLEBACKGROUNDTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, RESOLUTION)){
			ival = SendDlgItemMessage(hdlg, RESOLUTION, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, RESOLUTIONTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, DEPTH)){
			ival = SendDlgItemMessage(hdlg, DEPTH, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, DEPTHTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, FOV)){
			ival = SendDlgItemMessage(hdlg, FOV, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, FOVTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, GIZMOSPEED)){
			ival = SendDlgItemMessage(hdlg, GIZMOSPEED, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, GIZMOSPEEDTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, COLORSPEED)){
			ival = SendDlgItemMessage(hdlg, COLORSPEED, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, COLORSPEEDTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, CAMERASPEED)){
			ival = SendDlgItemMessage(hdlg, CAMERASPEED, TBM_GETPOS, 0, 0);
			sprintf_s(cval, "%d", ival);
			SendDlgItemMessage(hdlg, CAMERASPEEDTEXT, WM_SETTEXT, 0, LPARAM(cval));
		}
		if(HWND(lpm) == GetDlgItem(hdlg, FRAMERATELIMIT))
			updateFrameRateLimitSlider(hdlg, FRAMERATELIMIT, FRAMERATELIMITTEXT);
		return TRUE;
    }
    return FALSE;
}


// press two digits within a timeout period to choose a specific gizmo
void chooseSpecificGizmo(int n){
	static int spec_gizmo;

	if(n < 0 || n > 9)
		return;

	if(gNumberInputTimer <= 0.0f){  // 1st key pressed is 10's place digit
		spec_gizmo = n * 10;
		gNumberInputTimer = 3.0f;  // 3 seconds left to press 2nd digit
	}
	else{  // 2nd key pressed is 1's place digit
		spec_gizmo += n;
		if(spec_gizmo < gizmos.size() - 1)  // select any gizmo except easter egg gizmo
			gSpecificGizmo = spec_gizmo;
		gNumberInputTimer = 0.0f;
	}
}


LONG screenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm){
	typedef union{
		unsigned int i;
		unsigned char c[4];
	} uint_or_uchar;

	switch(msg){
	case WM_CREATE:
		readRegistry();
		initSaver(hwnd);
		break;
	case WM_KEYDOWN:
		switch(int(wpm)){
		// select specific gizmo by typing 2-digit number
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case VK_NUMPAD0:
		case VK_NUMPAD1:
		case VK_NUMPAD2:
		case VK_NUMPAD3:
		case VK_NUMPAD4:
		case VK_NUMPAD5:
		case VK_NUMPAD6:
		case VK_NUMPAD7:
		case VK_NUMPAD8:
		case VK_NUMPAD9:
			chooseSpecificGizmo((int(wpm) <= '9') ? (int(wpm) - '0') : (int(wpm) - VK_NUMPAD0));
			return 0;
		}
		// if numlock is not pressed, we can detect the scan codes, *not* the Win32 virtual key codes
		uint_or_uchar ioc;
		ioc.i = int(lpm);
		switch(int(ioc.c[2])){
		case 82:
			chooseSpecificGizmo(0);
			return 0;
		case 79:
			chooseSpecificGizmo(1);
			return 0;
		case 80:
			chooseSpecificGizmo(2);
			return 0;
		case 81:
			chooseSpecificGizmo(3);
			return 0;
		case 75:
			chooseSpecificGizmo(4);
			return 0;
		case 76:
			chooseSpecificGizmo(5);
			return 0;
		case 77:
			chooseSpecificGizmo(6);
			return 0;
		case 71:
			chooseSpecificGizmo(7);
			return 0;
		case 72:
			chooseSpecificGizmo(8);
			return 0;
		case 73:
			chooseSpecificGizmo(9);
			return 0;
		}
		break;
	case WM_DESTROY:
		readyToDraw = 0;
		cleanUp(hwnd);
		break;
	}
	return defScreenSaverProc(hwnd, msg, wpm, lpm);
}
#endif // WIN32
