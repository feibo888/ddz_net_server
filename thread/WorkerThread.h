#pragma once
#include <condition_variable>
#include "EventLoop.h"

class WorkerThread
{
public:
	WorkerThread(int index);
	~WorkerThread();

	//启动线程
	void run();
	inline EventLoop* getEventLoop()
	{
		return m_evLoop;
	}

private:
	void running();

private:
	thread::id m_threadID;
	string m_name;
	mutex m_mutex;		//互斥锁
	condition_variable m_cond;		//条件变量
	EventLoop* m_evLoop;	//反应堆模型
	thread* m_thread;		//保存线程的实例
};



