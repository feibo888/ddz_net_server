#pragma once
#include "DisPatcher.h"
#include "Chanel.h"
#include "EventLoop.h"
#include <sys/select.h>

class SelectDisPatcher : public DisPatcher
{
public:
	SelectDisPatcher(EventLoop* evLoop);
	~SelectDisPatcher();
	//添加
	int add() override;
	//删除
	int remove() override;
	//修改
	int modify() override;
	//事件检测
	int dispatch(int timeout = 2) override;

private:
	void setFdSet();
	void clearFdSet();

private:
	fd_set m_readSet;
	fd_set m_writeSet;
	const int m_maxNode = 1024;
};
