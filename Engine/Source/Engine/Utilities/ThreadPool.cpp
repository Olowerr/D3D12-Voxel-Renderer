#include "ThreadPool.h"

namespace Okay
{
	bool ThreadPool::s_stop = false;
	std::mutex ThreadPool::s_queueMutis;
	std::condition_variable ThreadPool::s_mutisCondition;
	std::vector<std::thread> ThreadPool::s_workerThreads;
	std::queue<std::function<void()>> ThreadPool::s_jobs;

	void ThreadPool::initialize(uint32_t numThreads)
	{
		s_workerThreads.reserve(numThreads);
		for (uint32_t i = 0; i < numThreads; i++)
		{
			s_workerThreads.emplace_back(ThreadPool::waitForJob);
		}
	}

	void ThreadPool::shutdown()
	{
		std::unique_lock lock(s_queueMutis);
		s_stop = true;
		lock.unlock();

		s_mutisCondition.notify_all();
		for (std::thread& thread : s_workerThreads)
			thread.join();
	}

	void ThreadPool::queueJob(const std::function<void()>& job)
	{
		std::unique_lock lock(s_queueMutis);
		s_jobs.push(job);
		lock.unlock();

		s_mutisCondition.notify_one();
	}

	void ThreadPool::waitForJob()
	{
		while (true)
		{
			std::unique_lock lock(s_queueMutis);
			s_mutisCondition.wait(lock, []()
				{
					return !s_jobs.empty() || s_stop;
				});
			if (s_stop)
				break;

			std::function<void()> job = s_jobs.front();
			s_jobs.pop();
			lock.unlock();

			job();
		}
	}
}
