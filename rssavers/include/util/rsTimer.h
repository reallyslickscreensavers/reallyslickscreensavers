/*
 * Copyright (C) 2004-2006  Terence M. Welsh
 *
 * This file is part of rsUtility.
 *
 * rsUtility is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
 *
 * rsUtility is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef RS_TIMER_H
#define RS_TIMER_H



#ifdef WIN32
#include <windows.h>
#else
#include <stdlib.h>
#include <sys/time.h>
#endif


class rsTimer{
public:
	float time;
#ifdef WIN32
	BOOL highResCounterSupported;
	float freq;  // high frequency system counts per second
	LONGLONG curr, prev;
	// for low res timer if high res is unavailable
	DWORD lowResCurr, lowResPrev;
#else
	struct timeval prev_tv;
#endif

	rsTimer(){
		time = 0.0f;
#ifdef WIN32
		// init high- and low-res timers
		LARGE_INTEGER n[1];
		highResCounterSupported = QueryPerformanceFrequency(n);
		freq = 1.0f / float(n[0].QuadPart);
		timeBeginPeriod(1);  // make Sleep() and timeGetTime() more accurate
		// get first tick for high- and low-res timers
		QueryPerformanceCounter(n);
		curr = n[0].QuadPart;
		lowResCurr = timeGetTime();
#else
		gettimeofday(&prev_tv, NULL);
#endif
	}

	~rsTimer(){}

	// return time elapsed since last call to tick()
	float tick(){
#ifdef WIN32
		if(highResCounterSupported){
			prev = curr;
			LARGE_INTEGER n[1];
			QueryPerformanceCounter(n);
			curr = n[0].QuadPart;
			if(curr >= prev) 
				time = float(curr - prev) * freq;
			// else use time from last frame
		}
		else{
			lowResPrev = lowResCurr;
			lowResCurr = timeGetTime();
			if(lowResCurr >= lowResPrev) 
				time = float(lowResCurr - lowResPrev) * 0.001f;
			// else use time from last frame
		}
#else
		struct timeval curr_tv;
		gettimeofday(&curr_tv, NULL);
		float time = (curr_tv.tv_sec - prev_tv.tv_sec)
			+ ((curr_tv.tv_usec - prev_tv.tv_usec) * 0.000001f);
		prev_tv = curr_tv;
#endif
		return time;
	}
};



#endif
