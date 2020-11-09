#include "Scheduler/Task.h"


Task::Task(const std::string &taskName, uint64_t taskSchedule) :
	schedule(taskSchedule),
	name(taskName)
{
}


Task::Task() :
	schedule(0)
{
}


Task::~Task()
{
}
