#include "TcpConnection.h"

#include <Codec.h>
#include <fstream>
#include <RsaCrypto.h>
#include <sstream>
#include <glog/logging.h>

#include "Room.h"


int TcpConnection::processRead(void* arg)
{
	TcpConnection* conn = static_cast<TcpConnection*>(arg);

	int socket = conn->m_channel->getSocket();
	//接收数据
	int count = conn->m_readBuf->socketRead(socket);

	if (count > 0)
	{
		//解析斗地主数据
		conn->m_reply->parseRequest(conn->m_readBuf);
	}
	else
	{
		//断开和客户端的连接
		conn->addDeleteTask();
		cout << "已断开连接" << endl;
	}


	return 0;
}

int TcpConnection::processWrite(void* arg)
{
	TcpConnection* conn = static_cast<TcpConnection*>(arg);

	//发送数据
	int count = conn->m_writeBuf->sendData(conn->m_channel->getSocket());

	if (count > 0)
	{
		//判断数据是否被全部发送出去了
		if (conn->m_writeBuf->readableSize() == 0)
		{
			//1. 不在检测写事件
			conn->m_channel->setCurrentEvent(FDEvent::ReadEvent);
			//2. 修改为只读事件
			conn->m_evLoop->addTask(conn->m_channel, ElemType::MODIFY);
		}
	}
	return 0;
}


int TcpConnection::destory(void* arg)
{
	TcpConnection* conn = static_cast<TcpConnection*>(arg);

	if (conn != nullptr)
	{
		delete conn;
	}

	return 0;
}

TcpConnection::TcpConnection(int fd, EventLoop* evLoop)
{
	m_evLoop = evLoop;
	m_readBuf = new Buffer(10240);
	m_writeBuf = new Buffer(10240);
	m_reply = new Communication();

	auto writeFunc = bind(&TcpConnection::addWriteTask, this, placeholders::_1);
	auto deleteFunc = bind(&TcpConnection::addDeleteTask, this);

	m_reply->setCallback(writeFunc, deleteFunc);

	m_name = "Connection-" + to_string(fd);

	//向读缓冲区中添加公钥分发数据
	prepareSecretKey();

	m_channel = new Channel(fd, (int)FDEvent::WriteEvent, processRead, processWrite, destory, this);
	m_evLoop->addTask(m_channel, ElemType::ADD);
}

TcpConnection::~TcpConnection()
{
	if (m_readBuf && m_readBuf->readableSize() == 0 &&
		m_writeBuf && m_writeBuf->readableSize() == 0)
	{
		delete m_readBuf;
		delete m_writeBuf;
		m_evLoop->destoryChannel(m_channel);
	}
}

void TcpConnection::addWriteTask(string data)
{
	m_writeBuf->appendPackage(data);
#if 1
	//添加事件进行发送
	//1. 检测写事件
	m_channel->setCurrentEvent(FDEvent::WriteEvent);
	//2. 修改为只读事件
	m_evLoop->addTask(m_channel, ElemType::MODIFY);

#else
	m_writeBuf->sendData(m_channel->getSocket());

#endif

}

void TcpConnection::addDeleteTask()
{
	m_evLoop->addTask(m_channel, ElemType::DELETE);
	LOG(INFO) << "和客户端断开连接: " << m_name;
}

void TcpConnection::prepareSecretKey()
{
	Room redis;
	redis.initEnvironment();

	//读公钥到字符串中
	std::string pubkey = redis.getRsaSecKey("PublicKey");

	//对公钥进行签名
	Message msg;
	msg.resCode = ResponseCode::RsaFenFa;
	msg.data1 = pubkey;

	//读私钥保存在rsa中，用于签名
	RsaCrypto rsa;
	rsa.parseStringToKey(redis.getRsaSecKey("PrivateKey"), RsaCrypto::PrivateKey);

	//签名
	std::string data = rsa.sign(pubkey);
	msg.data2 = data;

	//数据序列化
	Codec codec(&msg);
	data = codec.enCodeMsg();

	//写数据
	m_writeBuf->appendPackage(data);

}
