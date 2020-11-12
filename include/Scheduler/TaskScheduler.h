#pragma once
#include <vector>
#include <iostream>
#include <chrono>
#include <thread>
#include <deque>
#include <atomic>
#include <list>

#include "Thread.h"
#include "Task.h"
#include "../Time/StopWatch.h"


enum JobState
{
	None,
	Executing,
	Success
};

class Job
{
private:
	std::shared_ptr<Task> jobTask;
	std::shared_ptr<Thread> jobThread;

	std::mutex jobMutex;
	std::condition_variable wakeJob;

	bool isBound;
	std::atomic<JobState> state;
public:
	JobState getState() { return state; }
	void setState(JobState s) { state = s; }

	inline bool getIsBound() { return isBound; }

	const std::shared_ptr<Task>& getTask() { return jobTask; }
	void setTask(const std::shared_ptr<Task>& task) { jobTask = task; }

	const std::shared_ptr<Thread>& getThread() { return jobThread; }
	void setThread(const std::shared_ptr<Thread>& thread) { jobThread = thread; }

	std::mutex* getJobMutex() { return &jobMutex; }
	std::condition_variable* getJobSignal() { return &wakeJob; }

	static std::shared_ptr<Job> create(const std::shared_ptr<Task> &task, bool bound = false)
	{
		auto job = std::make_shared<Job>();
		job->isBound = bound;
		job->jobTask = task;
		return job;
	}

	Job();
	~Job();
};


class TaskScheduler
{
private:
	std::vector<std::shared_ptr<Thread>> threadPool;
	std::list<std::shared_ptr<Job>> jobs;
	std::list<std::shared_ptr<Task>> taskQueue;
	std::list<std::shared_ptr<Task>> executionQueue;

	std::shared_ptr<Thread> stepThread;

	StopWatch watch;

	std::condition_variable wakeWorker;
	std::condition_variable wakeScheduler;

	uint64_t steps;
	uint64_t totalTime;
	uint64_t tasksDispatched;

	double avgSchedulerTime;

	std::mutex schedulerMutex;
	std::mutex workerMutex;

	bool running;
protected:
	void jobThread(std::shared_ptr<Job> job);

	void workerThread(std::shared_ptr<Thread> thread);

	void step();

	void stopJobInternal(const std::shared_ptr<Job>& job);
public:
	bool isRunning() { return running; }

	void submitTask(const std::shared_ptr<Task>& task);

	void submitJob(const std::shared_ptr<Job>& job);

	void stopJob(const std::shared_ptr<Job>& job);

	void stopJob(const std::string& taskName);

	void pause() { running = false; }

	void resume() { running = true; }

	void close();

	void init(int threadCount);

	TaskScheduler();
	~TaskScheduler();
};

