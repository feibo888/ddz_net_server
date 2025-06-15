#include "HttpResponse.h"


HttpResponse::HttpResponse()
{
	m_statusCode = HttpStatusCode::UnKnown;
	m_headers.clear();
	m_fileName = string();
	m_sendDataFunc = nullptr;
}

HttpResponse::~HttpResponse()
{
}

void HttpResponse::addHeader(const string key, const string value)
{
	if (key.empty() || value.empty())
	{
		return;
	}
	m_headers.insert(make_pair(key, value));
}

void HttpResponse::prepareMsg(Buffer* sendBuf, int socket)
{
	//状态行
	char tmp[1024] = { 0 };
	sprintf(tmp, "HTTP/1.1 %d %s", m_statusCode, m_info.at((int)m_statusCode).data());
	sendBuf->appendString(tmp);
	//响应头
	for (auto it = m_headers.begin(); it != m_headers.end(); ++it)
	{
		sprintf(tmp, "%s: %s\r\n", it->first.data(), it->second.data());
		sendBuf->appendString(tmp);
	}
	//空行
	sendBuf->appendString("\r\n");


#ifndef _SEND_MSG_AUTO
	sendBuf->sendData(socket);
#endif // !_SEND_MSG_AUTO


	//回复的数据
	m_sendDataFunc(m_fileName, sendBuf, socket);
}
