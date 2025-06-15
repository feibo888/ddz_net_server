#pragma once

#include "EventLoop.h"
#include "ThreadPool.h"
#include <sys/types.h>          
#include <sys/socket.h>
#include <arpa/inet.h>
#include "TcpConnection.h"


class TcpServer
{
public:
	TcpServer(unsigned short port, int threadNum);
	~TcpServer();

	//初始化监听
	void setListen();
	//启动服务器
	void run();
	static int acceptConnection(void* arg);
	//保存rsa密钥到redis中
	void saveRsaKey();

private:
	int m_threadNum;
	EventLoop* m_mainLoop;
	ThreadPool* m_threadPool;
	int m_lfd;
	unsigned short m_port;
};

