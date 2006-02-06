#ifndef RS_TIMER_H
#define RS_TIMER_H



#include <sys/time.h>



class rsTimer{
public:
	struct timeval prev_tv;

	rsTimer(){
		gettimeofday(&prev_tv, NULL);
	}

	~rsTimer(){}

	// return time elapsed since last call to tick()
	float tick(){
		struct timeval curr_tv;
		gettimeofday(&curr_tv, NULL);
		float time = (curr_tv.tv_sec - prev_tv.tv_sec)
			+ ((curr_tv.tv_usec - prev_tv.tv_usec) * 0.000001f);
		prev_tv = curr_tv;
		return time;
	}
};



#endif
