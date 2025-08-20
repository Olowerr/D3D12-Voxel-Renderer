#pragma once
#include <thread>
#include <deque>
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
		ThreadPool() = default;
		~ThreadPool() = default;

		void initialize(uint32_t numThreads);
		void shutdown();
		void queueJob(const std::function<void()>& job);

	private:
		void waitForJob();

		bool m_stop = false;
		std::mutex m_queueMutis;
		std::condition_variable m_mutisCondition;
		std::vector<std::thread> m_workerThreads;
		std::deque<std::function<void()>> m_jobs;

	};
}
