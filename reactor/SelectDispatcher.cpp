#include "SelectDispatcher.h"


SelectDisPatcher::SelectDisPatcher(EventLoop* evLoop) : DisPatcher(evLoop)
{
	FD_ZERO(&m_readSet);
	FD_ZERO(&m_writeSet);
	m_name = "Select";
}

SelectDisPatcher::~SelectDisPatcher()
{
}

int SelectDisPatcher::add()
{
	if (m_channel->getSocket() >= m_maxNode)
	{
		return -1;
	}

	setFdSet();

	return 0;
}

int SelectDisPatcher::remove()
{
	clearFdSet();

	//通过channel释放对应的TcpConnection资源
	m_channel->m_destoryCallBack(const_cast<void*>(m_channel->getArg()));
	return 0;
}

int SelectDisPatcher::modify()
{
	FD_CLR(m_channel->getSocket(), &m_readSet);
	FD_CLR(m_channel->getSocket(), &m_writeSet);

	setFdSet();

	return 0;
}

int SelectDisPatcher::dispatch(int timeout)
{
	struct timeval val;
	val.tv_sec = timeout;
	val.tv_usec = 0;

	fd_set rdtmp = m_readSet;
	fd_set wrtmp = m_writeSet;

	int count = select(m_maxNode, &rdtmp, &wrtmp, NULL, &val);

	if (count == -1)
	{
		perror("select");
		exit(0);
	}

	for (int i = 0; i < m_maxNode; ++i)
	{
		//对端断开连接
		if (FD_ISSET(i, &rdtmp))
		{
			m_evLoop->eventActivate(i, (int)FDEvent::ReadEvent);
		}
		if (FD_ISSET(i, &wrtmp))
		{
			m_evLoop->eventActivate(i, (int)FDEvent::WriteEvent);
		}
	}
	return 0;
}

void SelectDisPatcher::setFdSet()
{
	if (m_channel->getEvents() & static_cast<int>(FDEvent::ReadEvent))
	{
		FD_SET(m_channel->getSocket(), &m_readSet);
	}
	if (m_channel->getEvents() & static_cast<int>(FDEvent::WriteEvent))
	{
		FD_SET(m_channel->getSocket(), &m_writeSet);
	}
	return;
}

void SelectDisPatcher::clearFdSet()
{
	if (m_channel->getEvents() & static_cast<int>(FDEvent::ReadEvent))
	{
		FD_CLR(m_channel->getSocket(), &m_readSet);
	}
	if (m_channel->getEvents() & static_cast<int>(FDEvent::WriteEvent))
	{
		FD_CLR(m_channel->getSocket(), &m_writeSet);
	}
	return;
}
