#pragma once
#include <thread>
#include <queue>
#include <condition_variable>
#include <functional>

namespace Okay
{
	/*
		Source:
		https://stackoverflow.com/questions/15752659/thread-pooling-in-c11
		https://www.geeksforgeeks.org/cpp/thread-pool-in-cpp
	*/

	class ThreadPool
	{
	public:
		ThreadPool() = delete;

		static void initialize(uint32_t numThreads);
		static void shutdown();
		static void queueJob(const std::function<void()>& job);

	private:
		static void waitForJob();

		static bool s_stop;
		static std::mutex s_queueMutis;
		static std::condition_variable s_mutisCondition;
		static std::vector<std::thread> s_workerThreads;
		static std::queue<std::function<void()>> s_jobs;

	};
}
