#pragma once

#include "DisPatcher.h"
#include "Chanel.h"
#include "SelectDispatcher.h"
#include "PollDispatcher.h"
#include "EpollDispatcher.h"
#include <thread>
#include <queue>
#include <map>
#include <mutex>
#include <string>

using namespace std;

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>




// 处理该节点中的channel的方式
enum class ElemType :char { ADD, DELETE, MODIFY };

//定义任务队列的节点
struct ChannelElement
{
	ElemType type;
	Channel* channel;
};

class DisPatcher;

class EventLoop
{
public:
	EventLoop();
	EventLoop(const string threadName);
	~EventLoop();

	//启动
	int run();
	//处理激活的fd
	int eventActivate(int fd, int event);

	//添加任务到任务队列
	int addTask(Channel* channel, ElemType type);

	//处理任务队列中的数据
	int processTaskQ();

	//处理dispatcher中的结点
	int add(Channel* channel);
	int remove(Channel* channel);
	int modify(Channel* channel);

	//释放channel
	int destoryChannel(Channel* channel);

	static int readLocalMessge(void* arg);
	int readMessge();

	inline thread::id getThreadID()
	{
		return m_threadID;
	}

private:
	void taskWakeup();

private:
	bool m_isQuit;
	DisPatcher* m_dispatcher;	//指向epoll, poll, select
	//任务列队
	queue<ChannelElement*> m_taskQ;
	//map
	map<int, Channel*> m_channelMap;
	//线程id,name,mutex
	thread::id m_threadID;
	string m_threadName;
	mutex m_mutex;
	int m_socketPair[2];	//存储本地通信的fd
};

