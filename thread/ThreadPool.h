#pragma once
#include "EventLoop.h"
#include "WorkerThread.h"
#include <vector>
#include <assert.h>
#include <memory>

class ThreadPool
{
public:
	ThreadPool(struct EventLoop* mainLoop, int count);
	~ThreadPool();

	//启动线程池
	void run();
	//取出线程池中的某个子线程的反应堆的实例
	EventLoop* takeWorkerEventLoop();

private:
	//主线程的反应堆模型
	EventLoop* m_mainLoop;
	bool m_isStart;
	int m_threadNum;
	vector<unique_ptr<WorkerThread>> m_workerThreads;
	int m_index;
};


