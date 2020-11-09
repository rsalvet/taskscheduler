#pragma once
#include <memory>
#include <string>
#include <mutex>

#include "../Time/StopWatch.h"


class Task
{
private:
	std::string name;
	uint64_t schedule;
	milliseconds timestamp;
public:
	const std::string& getName() { return name; }
	void setName(const std::string& str) { name = str; }

	uint64_t getSchedule() { return schedule; }
	void setSchedule(uint64_t s) { schedule = s; }

	const milliseconds& getTimestamp() { return timestamp; }
	void setTimestamp(const milliseconds& ms) { timestamp = ms; }
	void updateTimestamp() { timestamp = std::chrono::duration_cast<milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()); }

	virtual void invoke() {};

	void operator()()
	{
		invoke();
	}

	Task(const std::string &taskName, uint64_t taskSchedule);

	Task();
	~Task();
};


template <typename... Args>
class SchedulerTask : public Task
{
public:
	typedef void(*SchedulerFunction)(Args...);
private:
	SchedulerFunction taskFunction;
	std::tuple <Args...> arguments;
public:
	void invoke() override { std::apply(taskFunction, arguments); }
	void setFunction(const SchedulerFunction f) { taskFunction = f; }
	void setArguments(const std::tuple <Args...> &args) { arguments = args; }

	static std::shared_ptr <SchedulerTask<Args...>> create(const std::string& name, uint64_t schedule, const SchedulerFunction f, Args... args)
	{
		std::tuple<Args...> funcArguments = { args... };
		return std::make_shared<SchedulerTask<Args...>>(name, f, funcArguments, schedule);
	}

	SchedulerTask(const std::string& name, const SchedulerFunction execFunc, const std::tuple <Args...>& argsTuple, uint64_t schedule = 0) :
		Task(name, schedule),
		taskFunction(execFunc),
		arguments(argsTuple)
	{};

	SchedulerTask() {};
	~SchedulerTask() {};
};