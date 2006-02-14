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


/*
 * OpenGL screensaver library
 *
 * Library for handling standard OpenGL screensaver functionality, such
 * as opening windows, choosing pixel formats, setting video modes,
 * handling user input, etc...
 */



#ifndef RSX11SAVER_H
#define RSX11SAVER_H


#include <GL/glx.h>

#include "rsTimer.h"



extern int checkingPassword;  // A saver should check this and stop drawing if true
extern int isSuspended;  // power save mode
extern int doingPreview;  // Previews take extra long to initialize because Windows
// has lots of screwy resource hogging problems.  It's good to check this so that you
// can simplify your preview.

// These variables can be useful information for saver programmer
extern int pfd_swap_exchange;  // Is this bit set in the PIXELFORMATDESCRIPTOR?
extern int pfd_swap_copy;  // Is this bit set in the PIXELFORMATDESCRIPTOR?
// used to limit frame rate of saver (0 = no limit)
extern unsigned int dFrameRateLimit;
// Keyboard toggle for displaying statistics, such as frames per second.
// User must implement the displaying of statistics if kStatistics is true.
extern int kStatistics;


extern Display* xdisplay;
extern Window xwindow;



//----------------------------------------------------------------------------


// All idle processing is done here.  Typically, you would just draw 
// frames during this routine.
//void idleProc();



#endif
