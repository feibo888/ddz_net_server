#pragma once
#include "Buffer.h"
#include "TcpConnection.h"
#include <map>
#include <functional>

enum class HttpStatusCode
{
	UnKnown,
	OK = 200,
	MovedPermanently = 301,
	MovedTemporarily = 302,
	BadRequest = 400,
	NotFound = 404
};


class HttpResponse
{
public:
	HttpResponse();
	~HttpResponse();

	function<void(const string fileName, struct Buffer* writeBuf, int socket)> m_sendDataFunc;

	//添加响应头
	void addHeader(const string key, const string value);
	//组织http响应数据
	void prepareMsg(Buffer* sendBuf, int socket);
	inline void setFileName(string name)
	{
		m_fileName = name;
	}
	inline void setStatusCode(HttpStatusCode code)
	{
		m_statusCode = code;
	}

private:
	HttpStatusCode m_statusCode;
	string m_fileName;
	map<string, string> m_headers;		//响应头，键值对
	//定义状态码和描述的对应关系
	const map<int, string> m_info =
	{
		{200, "OK"},
		{301, "MovedPermanently"},
		{302, "MovedTemporarily"},
		{400, "BadRequest"},
		{404, "NotFound"},
	};
};



