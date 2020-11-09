#pragma once
#include <Windows.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

#include "../Time/StopWatch.h"


class Thread
{
private:
	std::thread* internalThread;
	std::atomic<bool> run;
	StopWatch lastAlive;
public:
	void setCanRun(bool v) { run = v; }
	inline bool canRun() { return run; }

	inline void update() { lastAlive.start(); }
	TimeUnit getLastAlive() { return lastAlive.getStartTime(); }

	void setOsThread(std::thread* thread) { internalThread = thread; }
	std::thread* getOsThread() { return internalThread; }

	Thread(std::thread* osThread);

	Thread();
	~Thread();
};

