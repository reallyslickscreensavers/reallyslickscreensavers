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
#include <rsX11Saver/vroot.h>

//#include <fstream>
//std::ofstream outfile;

int checkingPassword = 0;
int isSuspended = 0;
int doingPreview = 0;
unsigned int dFrameRateLimit = 0;
int kStatistics = 1;
Display* xdisplay;
Window xwindow = 0;
bool useRootWindow = false;

bool quit = false;


// Functions that must be implemented by screensaver
extern void handleCommandLine(int argc, char* argv[]);
extern void initSaver();
extern void cleanUp();
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
			switch(key){
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


int main(int argc, char* argv[]){
	//outfile.open("/home/terry/src/rssavers/src/Flux/outfile");
	int width = 512;
	int height = 480;

	/*for(int a=0; a<argc; a++){
		std::cout << argv[a] << std::endl;
		//outfile << argv[a] << std::endl;
	}*/

	if(-1 != findArgument(argc, argv, "-root")){
		useRootWindow = true;
	}

	int value;
	if(-1 != getArgumentsValue(argc, argv, std::string("-window-id"), value)){
		xwindow = value;
		//doingPreview = 1;
	}

	// get a connection
	if((xdisplay = XOpenDisplay(0)) == 0) {
		std::cout << "Cannot open display." << std::endl;
		exit (-1);
	}

	XVisualInfo *vis_info;
	if(useRootWindow || xwindow){
		if(xwindow == 0)
			xwindow = RootWindow(xdisplay, DefaultScreen(xdisplay));

		XWindowAttributes gwa;
		XGetWindowAttributes(xdisplay, xwindow, &gwa);
		Visual* visual = gwa.visual;
		width = gwa.width;
		height = gwa.height;

		XVisualInfo templ;
		templ.screen = DefaultScreen(xdisplay);
		templ.visualid = XVisualIDFromVisual(visual);

		int outCount;
		vis_info = XGetVisualInfo(xdisplay, VisualScreenMask | VisualIDMask,
				&templ, &outCount);

		if(!vis_info) {
			std::cerr << "Could not retrieve visual information for window: 0x"
				<< std::hex << xwindow << std::endl;
			exit (-1);
		}
	}
	else{
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
		if((vis_info = glXChooseVisual(xdisplay, DefaultScreen(xdisplay), attrs)) == 0){
			std::cout << "Cannot get visual." << std::endl;
			exit (-1);
		}

		// create a color map
		Colormap cmap;
		if((cmap = XCreateColormap(xdisplay, RootWindow(xdisplay, vis_info->screen),
			vis_info->visual, AllocNone)) == 0){
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

		xwindow = XCreateWindow(xdisplay, RootWindow(xdisplay, vis_info->screen),
			0, 0, width, height, 0, vis_info->depth, InputOutput, vis_info->visual,
			valuemask, &swa);
	}
	XMapWindow(xdisplay, xwindow);
	//XMapRaised(xdisplay, xwindow);

	// create a OpenGL rendering context
	GLXContext context;
	if((context = glXCreateContext(xdisplay, vis_info, 0, GL_TRUE)) == 0){
		std::cout << "Cannot create context." << std::endl;
		exit (-1);
	}

	// connect the context to the window
	glXMakeCurrent(xdisplay, xwindow, context);

	// initialize screensaver
	handleCommandLine(argc, argv);
	reshape(width, height);
	initSaver();

	// variables for limiting frame rate
	float desiredTimeStep = 0.0f;
	float timeRemaining = 0.0f;
	rsTimer timer;
	if(dFrameRateLimit)
		desiredTimeStep = 1.0f / float(dFrameRateLimit);

	// main loop
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

	// quit gracefully
	cleanUp();
	XCloseDisplay(xdisplay);
	//outfile.close();
}
