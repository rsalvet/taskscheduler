#include "Scheduler/Thread.h"


Thread::Thread(std::thread* osThread) :
	internalThread(osThread),
	run(false)
{
}

Thread::Thread() :
	internalThread(nullptr),
	run(false)
{
}


Thread::~Thread()
{
	if (internalThread)
	{
		if (internalThread->joinable())
			printf("thread wasn't joined before destructor");

		delete internalThread;
	}
}
