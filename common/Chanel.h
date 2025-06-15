#pragma once
#include <iostream>
#include <functional>
using namespace std;

//函数指针
//typedef int(*handleFunc)(void* arg);
//using handleFunc = int(*)(void*);

enum class FDEvent
{
	ReadEvent = 0x02,
	WriteEvent = 0x04
};

class Channel
{
public:
	using handleFunc = function<int(void*)>;
	Channel(int fd, int events, handleFunc readFunc, handleFunc writeFunc, handleFunc destoryFunc, void* arg);

	//回调函数
	handleFunc m_readCallback;
	handleFunc m_writeCallback;
	handleFunc m_destoryCallBack;

	//修改fd的写事件
	void writeEventEnable(bool flag);

	//对当前要检测的事件进行检测
	void setCurrentEvent(FDEvent ev);

	//判断是否需要检测文件描述符的写事件
	bool isWriteEventEnable();
	bool isReadEventEnable();

	inline int getSocket()
	{
		return m_fd;
	}

	inline int getEvents()
	{
		return m_events;
	}

	inline const void* getArg()
	{
		return m_arg;
	}

private:
	//文件描述符
	int m_fd;
	//事件
	int m_events;
	//回调函数的参数
	void* m_arg;
};

