#pragma once
#include "DisPatcher.h"
#include "Chanel.h"
#include "EventLoop.h"
#include <poll.h>

class PollDisPatcher : public DisPatcher
{
public:
	PollDisPatcher(EventLoop* evLoop);
	~PollDisPatcher();
	//添加
	int add() override;
	//删除
	int remove() override;
	//修改
	int modify() override;
	//事件检测
	int dispatch(int timeout = 2) override;

private:
	int m_maxfd;
	struct pollfd* m_fds;
	const int m_maxNode = 1024;
};
