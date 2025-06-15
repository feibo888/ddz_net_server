#pragma once
#include <string>
using namespace std;

class Channel;
class EventLoop;

class DisPatcher
{
public:
	DisPatcher(EventLoop* evLoop);
	virtual ~DisPatcher();
	//添加
	virtual int add();
	//删除
	virtual int remove();
	//修改
	virtual int modify();
	//事件检测
	virtual int dispatch(int timeout = 2);

	inline void setChannel(Channel* channel)
	{
		m_channel = channel;
	}

protected:
	string m_name = string();
	Channel* m_channel = nullptr;
	EventLoop* m_evLoop;

};
