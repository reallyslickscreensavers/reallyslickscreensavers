/*
 * Copyright (C) 2006  Terence M. Welsh
 *
 * This file is part of rsX11Saver.
 *
 * rsX11Saver is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
 *
 * rsX11Saver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <rsX11Saver/rsX11Saver.h>
#include <iostream>


int checkingPassword = 0;
int isSuspended = 0;
int doingPreview = 0;
unsigned int dFrameRateLimit = 0;
int kStatistics = 1;
Display* xdisplay;
Window xwindow;


extern void initSaver();
extern void idleProc();

	
	
// This goes with GLX window initialization boilerplate, from man GLXIntro
static int waitForNotify(Display *dsp, XEvent *event, char *arg) {
            return (event->type == MapNotify) && (event->xmap.window == (Window)arg);
}


int main(){
	GLXFBConfig *fbc;
	XVisualInfo *vis_info;
	Colormap cmap;
	XSetWindowAttributes swa;
	GLXContext context;
	XEvent event;

	// get a connection
	if ((xdisplay = XOpenDisplay(0)) == 0) {
		std::cout << "Cannot open display." << std::endl;
		exit (-1);
	}

	// get an appropriate visual
	static int attrs[] = {
		GLX_RGBA,
		GLX_DOUBLEBUFFER, True,
		GLX_DEPTH_SIZE, 24,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_ALPHA_SIZE, 8,
		None
		};
	int nelements;
	if ((vis_info = glXChooseVisual(xdisplay, DefaultScreen(xdisplay), attrs)) == 0) {
		std::cout << "Cannot get visual." << std::endl;
		exit (-1);
	}

	// create a GLX context
	if ((context = glXCreateContext(xdisplay, vis_info, 0, GL_TRUE)) == 0) {
		std::cout << "Cannot create context." << std::endl;
		exit (-1);
	}

	// create a color map
	if ((cmap = XCreateColormap(xdisplay, RootWindow(xdisplay, vis_info->screen),
		vis_info->visual, AllocNone)) == 0)
	{
		std::cout << "Cannot allocate colormap." << std::endl;
		exit (-1);
	}

 	// create a window
	swa.colormap = cmap;
	swa.border_pixel = 0;
	swa.event_mask = StructureNotifyMask;
	xwindow = XCreateWindow(xdisplay, RootWindow(xdisplay, vis_info->screen),
		0, 0, 500, 500, 0, vis_info->depth, InputOutput, vis_info->visual,
		CWBorderPixel|CWColormap|CWEventMask, &swa);
	XMapWindow(xdisplay, xwindow);
	XIfEvent(xdisplay, &event, waitForNotify, (char*)xwindow);

	// connect the context to the window
	glXMakeCurrent(xdisplay, xwindow, context);

	initSaver();

	// variables for limiting frame rate
	float desiredTimeStep = 0.0f;
	float timeRemaining = 0.0f;
	rsTimer timer;
	if(dFrameRateLimit)
		desiredTimeStep = 1.0f / float(dFrameRateLimit);

	while(1){
		if(isSuspended)  // don't waste cycles if saver is suspended
			sleep(1);
		if(dFrameRateLimit){  // frame rate is limited
			timeRemaining -= timer.tick();
			// don't allow underflow
			if(timeRemaining < -1000.0f)
				timeRemaining = 0.0f;
			if(timeRemaining > 0.0f){
				// wait some more
				if(timeRemaining > 0.001f)
					usleep(1000);
			}
			else{
				idleProc();  // do idle processing (i.e. draw frames)
				timeRemaining += desiredTimeStep;
			}
		}
		else{  // frame rate is unbound (draw as fast as possible)
			idleProc();  // do idle processing (i.e. draw frames)
			//usleep(1);
		}
	}
}
