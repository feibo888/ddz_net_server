#include "EpollDispatcher.h"


EpollDisPatcher::EpollDisPatcher(EventLoop* evLoop) : DisPatcher(evLoop)
{
	m_epfd = epoll_create(1);
	if (m_epfd == -1)
	{
		perror("epoll_create");
		exit(0);
	}
	m_events = new epoll_event[m_maxNode];
	m_name = "Epoll";
}

EpollDisPatcher::~EpollDisPatcher()
{
	close(m_epfd);
	delete[]m_events;
}

int EpollDisPatcher::add()
{
	int ret = epollCtl(EPOLL_CTL_ADD);

	if (ret == -1)
	{
		perror("epoll_ctl_add");
		exit(0);
	}
	return ret;
}

int EpollDisPatcher::remove()
{
	int ret = epollCtl(EPOLL_CTL_DEL);

	if (ret == -1)
	{
		perror("epoll_ctl_del");
		exit(0);
	}
	//通过channel释放对应的TcpConnection资源
	m_channel->m_destoryCallBack(const_cast<void*>(m_channel->getArg()));
	return ret;
}

int EpollDisPatcher::modify()
{
	int ret = epollCtl(EPOLL_CTL_MOD);

	if (ret == -1)
	{
		perror("epoll_ctl_mod");
		exit(0);
	}
	return ret;
}

int EpollDisPatcher::dispatch(int timeout)
{
	int count = epoll_wait(m_epfd, m_events, m_maxNode, timeout * 1000);

	for (int i = 0; i < count; ++i)
	{
		int events = m_events[i].events;
		int fd = m_events[i].data.fd;
		//对端断开连接
		if (events & EPOLLERR || events & EPOLLHUP)
		{
			//epollRemove(Channel, evLoop);
			continue;
		}
		if (events & EPOLLIN)
		{
			m_evLoop->eventActivate(fd, (int)FDEvent::ReadEvent);
		}
		if (events & EPOLLOUT)
		{
			m_evLoop->eventActivate(fd, (int)FDEvent::WriteEvent);
		}
	}
	return 0;
}

int EpollDisPatcher::epollCtl(int op)
{
	struct epoll_event ev;
	ev.data.fd = m_channel->getSocket();
	int events = 0;
	if (m_channel->getEvents() & static_cast<int>(FDEvent::ReadEvent))
	{
		events |= EPOLLIN;
	}
	if (m_channel->getEvents() & static_cast<int>(FDEvent::WriteEvent))
	{
		events |= EPOLLOUT;
	}

	ev.events = events;

	int ret = epoll_ctl(m_epfd, op, m_channel->getSocket(), &ev);

	return ret;
}
