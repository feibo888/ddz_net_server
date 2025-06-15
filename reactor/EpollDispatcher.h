#pragma once
#include "DisPatcher.h"
#include "Chanel.h"
#include "EventLoop.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <stdlib.h>


class EpollDisPatcher : public DisPatcher
{
public:
	EpollDisPatcher(EventLoop* evLoop);
	~EpollDisPatcher();
	//添加
	int add() override;
	//删除
	int remove() override;
	//修改
	int modify() override;
	//事件检测
	int dispatch(int timeout = 2) override;
private:
	int epollCtl(int op);

private:
	int m_epfd;
	epoll_event* m_events;
	const int m_maxNode = 520;
};