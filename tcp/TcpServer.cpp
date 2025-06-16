#include "TcpServer.h"

#include <fstream>
#include <RsaCrypto.h>
#include <sstream>
#include <glog/logging.h>

#include "Room.h"

int TcpServer::acceptConnection(void* arg)
{
	TcpServer* server = static_cast<TcpServer*>(arg);
	int cfd = accept(server->m_lfd, NULL, NULL);
	//从线程池中取出一个子线程的反应堆实例去处理这个cfd
	EventLoop* evLoop = server->m_threadPool->takeWorkerEventLoop();
	//将cfd放到TcpConnection中处理
	TcpConnection* conn = new TcpConnection(cfd, evLoop);

	LOG(INFO) << "TcpServer::acceptConnection, cfd: " << cfd << ", evLoop: " << evLoop->getThreadID();

	return 0;
}

void TcpServer::saveRsaKey()
{
	//生成密钥对
	RsaCrypto* rsa = new RsaCrypto;
	rsa->generateRsaKey(RsaCrypto::Bits_2K);
	delete rsa;

	//读密钥
	ifstream ifs("public.pem");
	stringstream sstr;
	sstr << ifs.rdbuf();
	string data = sstr.str();
	ifs.close();

	Room redis;
	assert(redis.initEnvironment());
	redis.clear();

	//保存公钥
	redis.saveRsaSecKey("PublicKey", data);

	ifs.open("private.pem");
	sstr << ifs.rdbuf();
	data = sstr.str();

	//保存私钥
	redis.saveRsaSecKey("PrivateKey", data);

	unlink("private.pem");
	unlink("public.pem");

}


TcpServer::TcpServer(unsigned short port, int threadNum)
{
	m_mainLoop = new EventLoop();
	m_threadNum = threadNum;
	m_port = port;
	m_threadPool = new ThreadPool(m_mainLoop, threadNum);

	setListen();
}

TcpServer::~TcpServer()
{
}

void TcpServer::setListen()
{
	LOG(INFO) << "start setListen";
	m_lfd = socket(AF_INET, SOCK_STREAM, 0);

	if (m_lfd == -1)
	{
		perror("socket");
		return;
	}

	int opt = 1;
	int ret = setsockopt(m_lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret == -1)
	{
		perror("setsockopt");
		return;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(m_port);
	addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(m_lfd, (struct sockaddr*)&addr, sizeof(addr));
	if (ret == -1)
	{
		perror("bind");
		return;
	}

	ret = listen(m_lfd, 128);
	if (ret == -1)
	{
		perror("listen");
		return;
	}

	LOG(INFO) << "setListen OK, lfd: " << m_lfd << " port: " << m_port;
}

void TcpServer::run()
{
	LOG(INFO) << "start run";
	//生成并保存rsa密钥到redis
	saveRsaKey();
	LOG(INFO) << "RSA Key Generation over";

	//启动线程池
	m_threadPool->run();
	LOG(INFO) << "threadPool run";

	//添加检测的任务
	Channel* channel = new Channel(m_lfd, (int)FDEvent::ReadEvent, acceptConnection, NULL, NULL, this);
	m_mainLoop->addTask(channel, ElemType::ADD);

	//启动反应堆模型
	m_mainLoop->run();
}
