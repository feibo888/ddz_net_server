#include "Buffer.h"

#include <netinet/in.h>

Buffer::Buffer(int size) : m_capacity(size)
{
	m_data = (char*)malloc(size);
	memset(m_data, 0, size);
}

Buffer::~Buffer()
{
	if (!m_data)
	{
		free(m_data);
	}
}

void Buffer::extendRoom(int size)
{
	//1. 内存够用，不需扩容
	if (writeableSize() >= size)
	{
		return;
	}
	//2. 内存合并才够用，不需扩容
	else if (m_readPos + writeableSize() >= size)
	{
		//得到未读的内存大小
		int readable = readableSize();
		//移动内存
		memcpy(m_data, m_data + m_readPos, readable);
		//更新位置
		m_readPos = 0;
		m_writePos = readable;
	}
	//3. 内存不够用，需要扩容
	else
	{
		void* tmp = realloc(m_data, m_capacity + size);
		if (tmp == NULL)
		{
			perror("realloc");
			return;
		}
		memset((char*)tmp + m_capacity, 0, size);
		//更新数据
		m_data = static_cast<char*>(tmp);
		m_capacity += size;
	}
}


int Buffer::appendString(const char* data, int size)
{
	if (m_data == nullptr || size <= 0)
	{
		return -1;
	}

	extendRoom(size);

	memcpy(m_data + m_writePos, data, size);
	m_writePos += size;

	return 0;
}

int Buffer::appendString(const char* data)
{
	int size = strlen(data);
	int ret = appendString(data, size);

	return ret;
}

int Buffer::appendString(const string data)
{
	int ret = appendString(data.data(), data.size());

	return ret;
}

int Buffer::appendHead(int length)
{
	//转换为大端字节序
	int len = htonl(length);
	//类型转换 int->string
	string head(reinterpret_cast<const char*>(&len), sizeof(len));
	appendString(head);
	return 0;
}

int Buffer::appendPackage(const string data)
{
	appendHead(data.size());
	appendString(data);
}

int Buffer::socketRead(int fd)
{
	struct iovec vec[2];
	int writeable = writeableSize();
	vec[0].iov_base = m_data + m_writePos;
	vec[0].iov_len = writeable;

	char* tmp = (char*)malloc(40960);
	vec[1].iov_base = tmp;
	vec[1].iov_len = 40960;

	int result = readv(fd, vec, 2);
	if (result == -1)
	{
		return -1;
	}
	else if (result <= writeable)
	{
		m_writePos += result;
	}
	else
	{
		m_writePos = m_capacity;
		appendString(tmp, result - writeable);
	}

	return result;
}

char* Buffer::findCRLF()
{
	char* ptr = static_cast<char*>(memmem(m_data + m_readPos, (size_t)readableSize(), "\r\n", 2));

	return ptr;
}

int Buffer::sendData(int socket)
{
	int readable = readableSize();
	if (readable > 0)
	{
		int count = send(socket, m_data + m_readPos, readable, MSG_NOSIGNAL);
		if (count > 0)
		{
			m_readPos += count;
			usleep(1);
		}
		return count;
	}
	return 0;
}
