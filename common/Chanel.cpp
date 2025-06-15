#include "Chanel.h"

Channel::Channel(int fd, int events, handleFunc readFunc, handleFunc writeFunc, handleFunc destoryFunc, void* arg)
{
	m_fd = fd;
	m_events = events;
	m_readCallback = readFunc;
	m_writeCallback = writeFunc;
	m_destoryCallBack = destoryFunc;
	m_arg = arg;
}

void Channel::writeEventEnable(bool flag)
{
	if (flag)
	{
		m_events |= static_cast<int>(FDEvent::WriteEvent);
	}
	else
	{
		m_events = m_events & ~static_cast<int>(FDEvent::WriteEvent);
	}
}

void Channel::setCurrentEvent(FDEvent ev)
{
	m_events = static_cast<int>(ev);
}

bool Channel::isWriteEventEnable()
{
	return m_events & static_cast<int>(FDEvent::WriteEvent);
}

bool Channel::isReadEventEnable()
{
	return m_events & static_cast<int>(FDEvent::ReadEvent);
}
