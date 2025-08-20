#include "ThreadPool.h"

namespace Okay
{
	void ThreadPool::initialize(uint32_t numThreads)
	{
		m_workerThreads.reserve(numThreads);
		for (uint32_t i = 0; i < numThreads; i++)
		{
			m_workerThreads.emplace_back(&ThreadPool::waitForJob, this);
		}
	}

	void ThreadPool::shutdown()
	{
		std::unique_lock lock(m_queueMutis);
		m_stop = true;
		lock.unlock();

		m_mutisCondition.notify_all();
		for (std::thread& thread : m_workerThreads)
			thread.join();
	}

	void ThreadPool::queueJob(const std::function<void()>& job)
	{
		std::unique_lock lock(m_queueMutis);
		m_jobs.push_back(job);
		lock.unlock();

		m_mutisCondition.notify_one();
	}

	void ThreadPool::waitForJob()
	{
		while (true)
		{
			std::unique_lock lock(m_queueMutis);
			m_mutisCondition.wait(lock, [&]()
				{
					return !m_jobs.empty() || m_stop;
				});
			if (m_stop)
				break;

			std::function<void()> job = m_jobs.front();
			m_jobs.pop_front();
			lock.unlock();

			job();
		}
	}
}
