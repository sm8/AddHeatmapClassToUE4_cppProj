#include "Timer.h"

unsigned int Timer::timeInMS(){
	return (unsigned int)(timeGetTime() - startT);
}

float Timer::timeInSec(){
	return (float)(timeInMS() / 1000.0f);
}

void Timer::reset(){
	startT = timeGetTime();
}
