#ifndef RS_TIMER_H
#define RS_TIMER_H



#include <windows.h>



// This is a handy timer class that does high resolution timing when possible
class rsTimer{
public:
	BOOL highResCounterSupported;
	float freq;  // high frequency system counts per second
	LONGLONG curr, prev;
	// for low res timer if high res is unavailable
	DWORD lowResCurr, lowResPrev;
	float time;

	rsTimer(){
		time = 0.0f;
		// init high- and low-res timers
		LARGE_INTEGER n[1];
		highResCounterSupported = QueryPerformanceFrequency(n);
		freq = 1.0f / float(n[0].QuadPart);
		timeBeginPeriod(1);  // make Sleep() and timeGetTime() more accurate
		// get first tick for high- and low-res timers
		QueryPerformanceCounter(n);
		curr = n[0].QuadPart;
		lowResCurr = timeGetTime();
	}

	~rsTimer(){}

	// return time elapsed since last call to tick()
	float tick(){
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
		return time;
	}
};



#endif
