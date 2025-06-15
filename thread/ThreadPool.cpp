#include "ThreadPool.h"

ThreadPool::ThreadPool(EventLoop* mainLoop, int count) :m_mainLoop(mainLoop), m_isStart(false), m_threadNum(count), m_index(0)
{
}

ThreadPool::~ThreadPool()
{
}

void ThreadPool::run()
{
	assert(!m_isStart);

	if (m_mainLoop->getThreadID() != this_thread::get_id())
	{
		exit(0);
	}

	m_isStart = true;
	if (m_threadNum > 0)
	{
		for (int i = 0; i < m_threadNum; ++i)
		{
			unique_ptr<WorkerThread> subThread(new WorkerThread(i));
			m_workerThreads.emplace_back(move(subThread));
			m_workerThreads.back()->run();
		}
	}
}

EventLoop* ThreadPool::takeWorkerEventLoop()
{
	assert(m_isStart);
	if (m_mainLoop->getThreadID() != this_thread::get_id())
	{
		exit(0);
	}

	//从线程池中找一个子线程，然后取出里面的反应堆实例
	EventLoop* evLoop = m_mainLoop;
	if (m_threadNum > 0)
	{
		evLoop = m_workerThreads[m_index]->getEventLoop();
		m_index = (m_index + 1) % m_threadNum;
	}

	return evLoop;
}
