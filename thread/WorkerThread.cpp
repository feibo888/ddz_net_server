#include "WorkerThread.h"

WorkerThread::WorkerThread(int index)
{
	m_threadID = thread::id();
	m_thread = nullptr;
	m_evLoop = nullptr;
	m_name = "subThread_" + to_string(index);
}

WorkerThread::~WorkerThread()
{
	if (m_thread != nullptr)
	{
		delete m_thread;
	}
}

void WorkerThread::run()
{
	//创建子线程
	m_thread = new thread(&WorkerThread::running, this);

	//阻塞主线程
	unique_lock<mutex> locker(m_mutex);
	while (m_evLoop == nullptr)
	{
		m_cond.wait(locker);
	}
}

void WorkerThread::running()
{
	m_mutex.lock();
	m_evLoop = new EventLoop(m_name);
	m_cond.notify_one();
	m_mutex.unlock();

	m_evLoop->run();
}
