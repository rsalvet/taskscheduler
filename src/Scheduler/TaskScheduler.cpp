#include "Scheduler/TaskScheduler.h"

#include <algorithm>


Job::Job() :
	isBound(false),
	state(JobState::None)
{

}

Job::~Job()
{
	if (jobThread)
	{
		jobThread->setCanRun(false);
		wakeJob.notify_one();
	}
}

void TaskScheduler::submitJob(const std::shared_ptr<Job>& job)
{
	{
		std::scoped_lock<std::mutex> lock(schedulerMutex);
		if (!running)
			throw std::runtime_error("can't add a new job because scheduler is not running");

		if (!job->getTask())
			throw std::runtime_error("job doesn't contain a task!");

		job->getTask()->updateTimestamp();

		if (job->getIsBound())
		{
			std::shared_ptr<Thread> thread = std::make_shared<Thread>();
			job->setThread(thread);
			thread->setCanRun(true);
			thread->setOsThread(new std::thread(&TaskScheduler::jobThread, this, job));
		}

		jobs.push_back(job);
	}

	wakeScheduler.notify_one();
}

void TaskScheduler::close()
{
	if (!running || !stepThread)
		return;

	{
		std::scoped_lock<std::mutex> schedulerLock(schedulerMutex);
		running = false;
	}

	wakeScheduler.notify_one();

	{
		std::scoped_lock<std::mutex> schedulerLock(schedulerMutex);
		std::scoped_lock<std::mutex> workerLock(workerMutex);

		stepThread->getOsThread()->join();
		stepThread.reset();

		taskQueue.clear();
		executionQueue.clear();

		for (auto thread : threadPool)
			thread->setCanRun(false);

		wakeWorker.notify_all();
	}

	std::list<std::shared_ptr<Job>> jobsCopy;

	{
		std::scoped_lock<std::mutex> lock(schedulerMutex);

		for (auto thread : threadPool)
			thread->getOsThread()->join();

		jobsCopy = jobs;

		threadPool.clear();
	}

	for (auto job : jobsCopy)
		stopJob(job);
}

void TaskScheduler::init(int threadCount)
{
	std::scoped_lock<std::mutex> lock(schedulerMutex);

	running = true;

	for (int i = 0; i < threadCount; ++i)
	{
		std::shared_ptr<Thread> thread = std::make_shared<Thread>();
		thread->setCanRun(true);
		thread->setOsThread(new std::thread(&TaskScheduler::workerThread, this, thread));
		threadPool.push_back(thread);
	}

	stepThread = std::make_shared<Thread>(new std::thread(&TaskScheduler::step, this));
}

void TaskScheduler::submitTask(const std::shared_ptr<Task>& task)
{
	{
		std::scoped_lock<std::mutex> lock(schedulerMutex);
		if (!running)
			throw std::runtime_error("can't add a new task because scheduler is not running");

		task->updateTimestamp();
		taskQueue.push_back(task);
	}

	wakeScheduler.notify_one();
}

void TaskScheduler::stopJobInternal(const std::shared_ptr<Job>& job)
{
	std::shared_ptr<Thread> jobThread = job->getThread();
	std::thread* osThread = jobThread->getOsThread();

	if (job->getIsBound())
	{
		{
			std::scoped_lock<std::mutex> lock(*job->getJobMutex());
			jobThread->setCanRun(false);
		}

		job->getJobSignal()->notify_one();
		osThread->join();

		jobThread->setOsThread(nullptr);
		delete osThread;
	}
}

void TaskScheduler::stopJob(const std::shared_ptr<Job>& job)
{
	std::scoped_lock<std::mutex> schedulerLock(schedulerMutex);

	auto it = std::find_if(jobs.begin(), jobs.end(), [&](const std::shared_ptr<Job>& vecJob) {
		return (vecJob == job);
	});

	if (it == jobs.end())
		throw std::runtime_error("couldn't stop job because it does not exist!");

	jobs.erase(it);
	stopJobInternal(job);
}

void TaskScheduler::stopJob(const std::string& taskName)
{
	std::scoped_lock<std::mutex> lock(schedulerMutex);
	std::list<std::shared_ptr<Job>>::iterator it;

	it = std::find_if(jobs.begin(), jobs.end(), [&](const std::shared_ptr<Job>& vecJob) {
		return (vecJob->getTask() && vecJob->getTask()->getName() == taskName);
	});

	if (it == jobs.end())
		throw std::runtime_error("couldn't stop job because it does not exist!");

	std::shared_ptr<Job> job = (*it);
	jobs.erase(it);

	stopJobInternal(job);
}

void TaskScheduler::jobThread(std::shared_ptr<Job> job)
{
	std::shared_ptr<Thread> thread = job->getThread();
	std::shared_ptr<Task> task = job->getTask();

	std::unique_lock<std::mutex> lock(*job->getJobMutex());
	lock.unlock();

	while (thread->canRun())
	{
		thread->update();

		lock.lock();

		job->getJobSignal()->wait(lock);

		if (!thread->canRun())
			return;

		lock.unlock();

		job->setState(JobState::Executing);

		task->invoke();

		job->setState(JobState::Success);
	}
}

void TaskScheduler::workerThread(std::shared_ptr<Thread> thread)
{
	std::unique_lock<std::mutex> workerLock(workerMutex);
	workerLock.unlock();

	while (thread->canRun())
	{
		thread->update();

		workerLock.lock();

		if (!thread->canRun())
			return;

		while (executionQueue.empty())
		{
			wakeWorker.wait(workerLock);
			if (!thread->canRun())
				return;
		}

		std::shared_ptr<Task> task = executionQueue.front();
		executionQueue.pop_front();
		workerLock.unlock();

		task->invoke();
	}
}

void TaskScheduler::step()
{
	const milliseconds timeToSleep = std::chrono::milliseconds(1);

	while (running)
	{
		std::unique_lock<std::mutex> schedulerLock(schedulerMutex);

		while (taskQueue.empty() && jobs.empty() && running)
			wakeScheduler.wait(schedulerLock);

		if (!running)
			break;

		watch.start();
		const TimeUnit &currentSchedulerTime = watch.getStartTime();

		for (auto it = taskQueue.begin(); it != taskQueue.end();) {
			std::shared_ptr<Task> task = (*it);

			int64_t schedule = task->getSchedule();
			int64_t elapsed = (std::chrono::duration_cast<milliseconds>(currentSchedulerTime.time_since_epoch())
				- task->getTimestamp()).count();

			if (elapsed >= schedule) {

				{
					std::scoped_lock<std::mutex> workerLock(workerMutex);

					executionQueue.push_back(task);
					it = taskQueue.erase(it);
					task->updateTimestamp();
				}

				tasksDispatched++;

				wakeWorker.notify_one();

				continue;
			}

			if (it == taskQueue.end())
				break;

			it = std::next(it, 1);
		}

		for (auto job : jobs)
		{
			std::shared_ptr<Task> task = job->getTask();
			if (!task)
				continue;

			int64_t schedule = task->getSchedule();
			int64_t elapsed = (std::chrono::duration_cast<milliseconds>(currentSchedulerTime.time_since_epoch())
				- task->getTimestamp()).count();

			if (elapsed >= schedule)
			{
				if (job->getState() == JobState::Executing)
					continue;

				if (job->getIsBound())
				{
					job->getJobSignal()->notify_one();
				}
				else
				{
					{
						std::scoped_lock<std::mutex> workerLock(workerMutex);
						executionQueue.push_back(task);
					}

					wakeWorker.notify_one();
				}

				task->updateTimestamp();

				tasksDispatched++;
			}
		}

		schedulerLock.unlock();
		steps++;

		milliseconds timeTaken = watch.getDuration();
		totalTime += timeTaken.count();
		avgSchedulerTime = (double)(totalTime / steps);

		std::this_thread::sleep_for(timeToSleep);
	}
}

TaskScheduler::TaskScheduler() :
	steps(0),
	totalTime(0),
	avgSchedulerTime(0),
	tasksDispatched(0),
	running(false)
{
}


TaskScheduler::~TaskScheduler()
{
	close();
}
