#include "DisPatcher.h"
#include <poll.h>
#include <stdlib.h>
#include "PollDispatcher.h"

PollDisPatcher::PollDisPatcher(EventLoop* evLoop) : DisPatcher(evLoop)
{
	m_maxfd = 0;
	m_fds = new pollfd[m_maxNode];

	for (int i = 0; i < m_maxNode; ++i)
	{
		m_fds[i].fd = -1;
		m_fds[i].events = 0;
		m_fds[i].revents = 0;
	}
	m_name = "Poll";
}

PollDisPatcher::~PollDisPatcher()
{
	delete[]m_fds;
}

int PollDisPatcher::add()
{
	int events = 0;
	if (m_channel->getEvents() & static_cast<int>(FDEvent::ReadEvent))
	{
		events |= POLLIN;
	}
	if (m_channel->getEvents() & static_cast<int>(FDEvent::WriteEvent))
	{
		events |= POLLOUT;
	}

	int i = 0;
	for (; i < m_maxNode; ++i)
	{
		if (m_fds[i].fd == -1)
		{
			m_fds[i].fd = m_channel->getSocket();
			m_fds[i].events = events;
			m_maxfd = i > m_maxfd ? i : m_maxfd;
			break;
		}
	}
	if (i >= m_maxNode)
	{
		return -1;
	}

	return 0;
}

int PollDisPatcher::remove()
{
	int i = 0;
	for (; i < m_maxNode; ++i)
	{
		if (m_fds[i].fd == m_channel->getSocket())
		{
			m_fds[i].fd = -1;
			m_fds[i].events = 0;
			m_fds[i].revents = 0;
			break;
		}
	}
	//通过channel释放对应的TcpConnection资源
	m_channel->m_destoryCallBack(const_cast<void*>(m_channel->getArg()));

	if (i >= m_maxNode)
	{
		return -1;
	}

	return 0;
}

int PollDisPatcher::modify()
{
	int events = 0;
	if (m_channel->getEvents() & static_cast<int>(FDEvent::ReadEvent))
	{
		events |= POLLIN;
	}
	if (m_channel->getEvents() & static_cast<int>(FDEvent::WriteEvent))
	{
		events |= POLLOUT;
	}

	int i = 0;
	for (; i < m_maxNode; ++i)
	{
		if (m_fds[i].fd == m_channel->getSocket())
		{
			m_fds[i].events = events;
			break;
		}
	}
	if (i >= m_maxNode)
	{
		return -1;
	}

	return 0;
}

int PollDisPatcher::dispatch(int timeout)
{
	int count = poll(m_fds, m_maxfd + 1, timeout * 1000);

	if (count == -1)
	{
		perror("poll");
		exit(0);
	}

	for (int i = 0; i <= m_maxfd; ++i)
	{
		//对端断开连接
		if (m_fds[i].fd == -1)
		{
			continue;
		}
		if (m_fds[i].revents & POLLIN)
		{
			m_evLoop->eventActivate(m_fds[i].fd, (int)FDEvent::ReadEvent);
		}
		if (m_fds[i].revents & POLLOUT)
		{
			m_evLoop->eventActivate(m_fds[i].fd, (int)FDEvent::WriteEvent);
		}
	}
	return 0;
}
