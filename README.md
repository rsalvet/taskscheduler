```cpp
std::shared_ptr<TaskScheduler> scheduler = std::make_shared<TaskScheduler>();
scheduler->init(4); // number of workers

int num = 555;

scheduler->submitTask(
	SchedulerTask<int>::create("testTask", 100, [](int abc) {
		printf("%d\n", abc);
	}, num)
);

scheduler->submitJob(
	Job::create(SchedulerTask<>::create("testJob", 1000, []() {
		printf("hi\n");
	}), true) // if true then the job executes the task in a separate thread
);

std::this_thread::sleep_for(std::chrono::seconds(10));

printf("stop the job!\n");
scheduler->stopJob("testJob"); // this is a blocking operation
printf("job stopped\n");
```