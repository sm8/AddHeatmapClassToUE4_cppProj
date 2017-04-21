#pragma once

#include <windows.h>
#include <stdio.h>
#include <mmsystem.h>  //for timer functions
//Note: Library winmm.lib must be added to linker input

class Timer{
	DWORD startT;
public:
	Timer(){ reset(); }
	unsigned int timeInMS(); 
	float timeInSec();
	void reset();
};

