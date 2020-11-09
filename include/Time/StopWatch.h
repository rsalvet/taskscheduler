#pragma once
#include <chrono>
#include <Windows.h>
#include <stdio.h>


typedef std::chrono::high_resolution_clock high_resolution_clock;
typedef std::chrono::milliseconds milliseconds;
typedef high_resolution_clock::time_point TimeUnit;


class StopWatch
{
private:
	TimeUnit startTime;
public:
	void start() { startTime = high_resolution_clock::now(); }
	const TimeUnit &getStartTime() { return startTime; }

	milliseconds getDuration()
	{
		milliseconds elapsed = std::chrono::duration_cast<milliseconds>(high_resolution_clock::now() - startTime);
		return elapsed;
	}

	StopWatch();
	~StopWatch();
};

