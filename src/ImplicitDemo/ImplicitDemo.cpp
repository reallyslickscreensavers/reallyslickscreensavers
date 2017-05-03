/*
 * Copyright (C) 2001-2010  Terence M. Welsh
 *
 * This file is part of Implicit.
 *
 * Implicit is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Implicit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef WIN32
	#include <sys/time.h>
#endif

#include <iostream>
#include <GL/glut.h>
#include <rsMath/rsMath.h>
#include <Implicit/impCubeVolume.h>
#include <Implicit/impCrawlPoint.h>
#include <Implicit/impEllipsoid.h>
#include <Implicit/impHexahedron.h>
#include <Implicit/impRoundedHexahedron.h>
#include <Implicit/impKnot.h>
#include <Implicit/impSphere.h>
#include <Implicit/impTorus.h>
#include <ImplicitDemo/texture.h>
#include <util/rsTimer.h>


impCubeVolume* volume;
impSurface* surface;


impSphere sphere;
impEllipsoid ellipsoid;
impTorus torus1, torus2;
impHexahedron hexa;
impRoundedHexahedron roundhexa;
impKnot knot;



void reshape(int width, int height){
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, float(width)/float(height), 0.1f, 100.0f);
}


void initOpengl(){
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
    
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	float mat_diffuse[] = {1.0f, 1.0f, 1.0f, 1.0f};
	float mat_none[] = {0.0f, 0.0f, 0.0f, 1.0f};
	glMaterialfv(GL_FRONT, GL_AMBIENT, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_EMISSION, mat_none);
	glMaterialf(GL_FRONT, GL_SHININESS, 40.0f);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
	float light0_ambient[] = {0.0f, 0.0f, 0.0f, 1.0f};
	float light0_diffuse[] = {1.0f, 1.0f, 1.0f, 1.0f};
	float light0_specular[] = {0.3f, 0.3f, 0.3f, 1.0f};
	float light0_direction[] = {8.0f, 8.0f, 12.0f, 0.0f};
	glLightfv(GL_LIGHT0, GL_AMBIENT, light0_ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light0_specular);
	glLightfv(GL_LIGHT0, GL_POSITION, light0_direction);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, mat_none);

	glBindTexture(GL_TEXTURE_2D, 1);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, TEXSIZE, TEXSIZE, GL_RGBA, 
		GL_UNSIGNED_BYTE, spheremap);

	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glEnable(GL_TEXTURE_2D);
}


void display(){
	static float matrix[16];
	static float center[3];
	static float move[8] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
	static rsMatrix mat1, mat2;
	double frameTime;
	static int printFPS = 0;
	static double totalTime = 0.0f;
	static double computeTime = 0.0f;
	static double drawTime = 0.0f;

	static rsTimer computeTimer, drawTimer;
	// start compute time timer
	computeTimer.tick();

	static rsTimer timer;
	frameTime = float(timer.tick());

	move[0] += float(frameTime) * 0.3f;
	move[1] += float(frameTime) * 0.5f;
	move[2] += float(frameTime) * 0.7f;
	move[3] += float(frameTime) * 1.1f;
	move[4] += float(frameTime) * 1.3f;
	move[5] += float(frameTime) * 1.5f;
	move[6] += float(frameTime) * 1.7f;
	move[7] += float(frameTime) * 2.1f;

	sphere.setPosition(cosf(move[4]), sinf(move[5]), cosf(move[6]));
	sphere.setThickness(0.3f + 0.15f * cosf(move[7]));

	mat1.makeScale(3.0f + 1.5f * cosf(move[7] * 4.0f),
		3.0f + 1.5f * cosf(move[7] * 4.0f),
		3.0f + 1.5f * cosf(move[7] * 4.0f + 1.5707f));
	mat2.makeRotate(move[2], 1.0f, 0.0f, 0.0f);
	mat1.postMult(mat2);
	mat2.makeRotate(move[4], 0.0f, 1.0f, 0.0f);
	mat1.postMult(mat2);
	mat2.makeTranslate(cosf(move[5]), cosf(move[6]), sinf(move[3]));
	mat1.postMult(mat2);
	mat1.get(matrix);
	ellipsoid.setMatrix(matrix);

	mat1.makeRotate(move[0], 1.0f, 0.0f, 0.0f);
	mat2.makeRotate(move[1], 0.0f, 1.0f, 0.0f);
	mat1.preMult(mat2);
	mat1.get(matrix);
	torus1.setMatrix(matrix);
	torus1.setRadius(1.1f);
	torus1.setThickness(0.17f);

	mat1.makeRotate(move[2], 0.0f, 1.0f, 0.0f);
	mat2.makeRotate(move[3], 1.0f, 0.0f, 0.0f);
	mat1.postMult(mat2);
	mat1.get(matrix);
	torus2.setMatrix(matrix);
	torus2.setRadius(1.5f);
	torus2.setThickness(0.17f);

	mat1.makeScale(0.25f, 0.25f, 0.25f + 0.15f * cosf(move[0]));
	mat2.makeRotate(move[3], 1.0f, 0.0f, 0.0f);
	mat1.postMult(mat2);
	mat2.makeRotate(move[5], 0.0f, 1.0f, 0.0f);
	mat1.postMult(mat2);
	mat2.makeTranslate(sinf(move[2]), cosf(move[5]), cosf(move[6]));
	mat1.postMult(mat2);
	mat1.get(matrix);
	hexa.setMatrix(matrix);

	mat1.makeRotate(move[7], 1.0f, 0.0f, 0.0f);
	mat2.makeRotate(move[5], 0.0f, 1.0f, 0.0f);
	mat1.postMult(mat2);
	mat2.makeTranslate(cosf(move[6]), sinf(move[4]), sinf(move[3]));
	mat1.postMult(mat2);
	mat1.get(matrix);
	roundhexa.setMatrix(matrix);
	roundhexa.setThickness(0.15f);
	roundhexa.setSize(0.2f + 0.1f * cosf(move[1]), 0.2f + 0.1f * cosf(move[0]), 0.2f);

	mat1.makeRotate(move[1], 1.0f, 0.0f, 0.0f);
	mat2.makeRotate(move[2], 0.0f, 1.0f, 0.0f);
	mat1.preMult(mat2);
	mat1.get(matrix);
	knot.setMatrix(matrix);
	//knot.setNumCoils(3);
	//knot.setNumTwists(4);

	impCrawlPointVector cpv;
	sphere.addCrawlPoint(cpv);
	ellipsoid.addCrawlPoint(cpv);
	torus1.addCrawlPoint(cpv);
	torus2.addCrawlPoint(cpv);
	hexa.addCrawlPoint(cpv);
	roundhexa.addCrawlPoint(cpv);
	knot.addCrawlPoint(cpv);

	surface->reset();
	volume->setSurfaceValue(0.5f);
	//volume->makeSurface();
	//volume->makeSurface(0,0,0);
	volume->makeSurface(cpv);
	//volume->makeSurface(0,0,0,cpv);

	// measure compute time
	computeTime += computeTimer.tick();
	// start draw time timer
	drawTimer.tick();

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0f, 0.0f, -6.0f);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	surface->draw();
	//surface->draw_wireframe();

	glFlush();
	glutSwapBuffers();
	glutPostRedisplay();

	// measure draw time
	drawTime += drawTimer.tick();

	// print timing info
	totalTime += frameTime;
	++printFPS;
	if(printFPS == 2000){
		std::cout << "FPS = " << 2000.0 / totalTime << std::endl;
		std::cout << "  compute = " << computeTime / 2000.0 << std::endl;
		std::cout << "     draw = " << drawTime / 2000.0 << std::endl;
		printFPS = 0;
		totalTime = 0.0f;
		computeTime = 0.0f;
		drawTime = 0.0f;
	}
}


float function(float* position){
	return(sphere.value(position)
		+ ellipsoid.value(position)
		+ torus1.value(position)
		+ torus2.value(position)
		//+ hexa.value(position)
		+ roundhexa.value(position)
		//+ knot.value(position)
	);
}


int main(int argc,  char **argv){
	glutInit(&argc, argv);
	glutInitWindowSize(400, 400);
	glutInitWindowPosition(100,  100);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	int windowID = glutCreateWindow("Implicit surface test");

	glutReshapeFunc(reshape);

	glutSetWindow(windowID);
	glutDisplayFunc(display);

	initOpengl();

	volume = new impCubeVolume;
	volume->init(50, 50, 50, 0.08f);
	volume->function = function;
	//volume->useFastNormals(false);
	//volume->setCrawlFromSides(true);
	surface = volume->getSurface();

	glutMainLoop();
    
	glutDestroyWindow(windowID);

	return 0;
}
