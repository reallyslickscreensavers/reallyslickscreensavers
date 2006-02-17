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
#include <X11/keysym.h>


int checkingPassword = 0;
int isSuspended = 0;
int doingPreview = 0;
unsigned int dFrameRateLimit = 0;
int kStatistics = 1;
Display* xdisplay;
Window xwindow;

bool quit = false;


extern void initSaver();
extern void idleProc();
extern void reshape(int, int);

	

void handleEvents(){
	XEvent event; 
	KeySym key; 
 
	while(XPending(xdisplay)){ 
		XNextEvent(xdisplay, &event); 
		switch(event.type){ 
		case KeyPress: 
			XLookupString((XKeyEvent *)&event, NULL, 0, &key, NULL); 
			switch(key) { 
			case XK_Escape: 
				quit = true;
				break; 
			} 
			break; 
		case ConfigureNotify: 
			reshape(event.xconfigure.width, event.xconfigure.height); 
			break; 
		}
	}
}

// This goes with GLX window initialization boilerplate, from man GLXIntro
static int waitForNotify(Display *dsp, XEvent *event, char *arg) {
            return (event->type == MapNotify) && (event->xmap.window == (Window)arg);
}


int main(){
	GLXFBConfig *fbc;
	XVisualInfo *vis_info;
	Colormap cmap;
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

	// create a OpenGL rendering context
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
	XSetWindowAttributes swa;
	unsigned long valuemask(0);
	swa.colormap = cmap;
	valuemask |= CWColormap;
	swa.border_pixel = 0;
	valuemask |= CWBorderPixel;
	swa.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;
	valuemask |= CWEventMask;
	
	{
		swa.cursor = None;
		valuemask |= CWCursor;
	}
	
	int width = 512;
	int height = 480;
	xwindow = XCreateWindow(xdisplay, RootWindow(xdisplay, vis_info->screen),
		0, 0, width, height, 0, vis_info->depth, InputOutput, vis_info->visual,
		valuemask, &swa);
	XMapWindow(xdisplay, xwindow);
	XIfEvent(xdisplay, &event, waitForNotify, (char*)xwindow);

	// connect the context to the window
	glXMakeCurrent(xdisplay, xwindow, context);

	reshape(width, height);
	initSaver();

	// variables for limiting frame rate
	float desiredTimeStep = 0.0f;
	float timeRemaining = 0.0f;
	rsTimer timer;
	if(dFrameRateLimit)
		desiredTimeStep = 1.0f / float(dFrameRateLimit);

	while(false == quit){ 
 		handleEvents();

		// don't waste cycles if saver is suspended
		if(isSuspended)
			sleep(1);

		// idle processing
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
