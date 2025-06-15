#pragma once
#include "EventLoop.h"
#include "Buffer.h"
#include "Chanel.h"
#include "Communication.h"



//#define _SEND_MSG_AUTO

class TcpConnection
{
public:
	TcpConnection(int fd, EventLoop* evLoop);
	~TcpConnection();

	//发送数据
	void addWriteTask(string data);
	//释放资源
	void addDeleteTask();

	//准备密钥
	void prepareSecretKey();
	static int processRead(void* arg);
	static int processWrite(void* arg);
	static int destory(void* arg);

private:
	EventLoop* m_evLoop;
	Channel* m_channel;
	Buffer* m_readBuf;
	Buffer* m_writeBuf;
	string m_name;
	Communication* m_reply = nullptr;
};

