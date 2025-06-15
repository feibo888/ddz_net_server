#include "EventLoop.h"


EventLoop::EventLoop() : EventLoop(string())
{
}

EventLoop::EventLoop(const string threadName)
{
	m_isQuit = true;	//默认没有启动
	m_threadID = this_thread::get_id();

	m_threadName = threadName == string() ? "MainThread" : threadName;
	m_dispatcher = new EpollDisPatcher(this);
	//m_dispatcher = new PollDisPatcher(this);
	//m_dispatcher = new SelectDisPatcher(this);

	//map
	m_channelMap.clear();

	int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, m_socketPair);
	if (ret == -1)
	{
		perror("socketpair");
		exit(0);
	}

#if 0
	//指定规则 evLoop->socketPair[0] 发送数据 evLoop->socketPair[1] 接收数据
	Channel* channel = new Channel(m_socketPair[1], static_cast<int>(FDEvent::ReadEvent), readLocalMessge, nullptr, nullptr, this);
#else
	function<int(void*)> obj = bind(&EventLoop::readMessge, this);
	Channel* channel = new Channel(m_socketPair[1], static_cast<int>(FDEvent::ReadEvent), obj, nullptr, nullptr, this);
#endif

	addTask(channel, ElemType::ADD);
}

EventLoop::~EventLoop()
{
}

int EventLoop::run()
{
	m_isQuit = false;
	//比较线程ID是否正常
	if (m_threadID != this_thread::get_id())
	{
		return -1;
	}
	//循环进行事件处理
	while (!m_isQuit)
	{
		m_dispatcher->dispatch();
		processTaskQ();
	}

	return 0;
}

int EventLoop::eventActivate(int fd, int event)
{
	if (fd < 0)
	{
		return -1;
	}
	//取出channel
	Channel* channel = m_channelMap[fd];
	assert(channel->getSocket() == fd);
	if (event & (int)FDEvent::ReadEvent)
	{
		channel->m_readCallback(const_cast<void*>(channel->getArg()));
	}
	if (event & (int)FDEvent::WriteEvent)
	{
		channel->m_writeCallback(const_cast<void*>(channel->getArg()));
	}

	return 0;
}

int EventLoop::addTask(Channel* channel, ElemType type)
{
	m_mutex.lock();

	//创建新节点
	ChannelElement* node = new ChannelElement;

	node->type = type;
	node->channel = channel;
	m_taskQ.push(node);

	m_mutex.unlock();

	if (m_threadID == this_thread::get_id())
	{
		//如果是子线程
		processTaskQ();
	}
	else
	{
		//如果是主线程，告诉子线程处理任务队列中的任务
		taskWakeup();
	}

	return 0;
}

int EventLoop::processTaskQ()
{
	while (!m_taskQ.empty())
	{
		m_mutex.lock();
		ChannelElement* node = m_taskQ.front();
		m_taskQ.pop();
		m_mutex.unlock();

		Channel* channel = node->channel;

		if (node->type == ElemType::ADD)
		{
			add(channel);
		}
		else if (node->type == ElemType::DELETE)
		{
			remove(channel);
		}
		else if (node->type == ElemType::MODIFY)
		{
			modify(channel);
		}

		delete node;
	}
	return 0;
}

int EventLoop::add(Channel* channel)
{
	int fd = channel->getSocket();

	if (m_channelMap.find(fd) == m_channelMap.end())
	{
		m_channelMap.insert(make_pair(fd, channel));
		m_dispatcher->setChannel(channel);
		int ret = m_dispatcher->add();
		return ret;
	}
	return -1;
}

int EventLoop::remove(Channel* channel)
{
	int fd = channel->getSocket();

	if (m_channelMap.find(fd) == m_channelMap.end())
	{
		return -1;
	}
	m_dispatcher->setChannel(channel);
	int ret = m_dispatcher->remove();

	return ret;
}

int EventLoop::modify(Channel* channel)
{
	int fd = channel->getSocket();

	if (m_channelMap.find(fd) == m_channelMap.end())
	{
		return -1;
	}
	m_dispatcher->setChannel(channel);
	int ret = m_dispatcher->modify();

	return ret;
}

int EventLoop::destoryChannel(Channel* channel)
{
	auto it = m_channelMap.find(channel->getSocket());
	if (it != m_channelMap.end())
	{
		m_channelMap.erase(it);
		close(channel->getSocket());
		delete channel;
		return 0;
	}
	return -1;
}

int EventLoop::readLocalMessge(void* arg)
{
	EventLoop* evLoop = static_cast<EventLoop*>(arg);
	char buf[256];
	read(evLoop->m_socketPair[1], buf, sizeof(buf));
	return 0;
}

int EventLoop::readMessge()
{
	char buf[256];
	read(m_socketPair[1], buf, sizeof(buf));
	return 0;
}

void EventLoop::taskWakeup()
{
	const char* msg = "wake up";
	write(m_socketPair[0], msg, strlen(msg));
}
