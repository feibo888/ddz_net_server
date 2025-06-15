#pragma once
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "Buffer.h"
#include <assert.h>
#include "HttpResponse.h"
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#include <map>
#include <functional>
using namespace std;

class HttpResponse;

enum class processState :char
{
	ParseReqLine,
	ParseReqHeaders,
	ParseReqBody,
	ParseReqDone
};


class HttpRequest
{
public:
	HttpRequest();
	~HttpRequest();


	//重置
	void reset();
	//获取http状态
	inline processState getState()
	{
		return m_curState;
	}
	inline void setStatus(processState status)
	{
		m_curState = status;
	}

	//添加请求头
	void addHeader(const string key, const string value);
	//获取value
	string getHeader(const string key);
	//解析请求行
	bool parseRequestLine(Buffer* readbuf);
	//解析请求头
	bool parseRequestHeader(Buffer* readbuf);
	//解析http请求协议
	bool parseHttpRequest(Buffer* readbuf,
		HttpResponse* response, Buffer* sendBuf, int socket);
	//处理http请求协议
	bool processHttpRequest(HttpResponse* response);
	string decodeMsg(string msg);
	const string getFileType(const string name);
	void sendFile(const string fileName, Buffer* sendBuf, int cfd);
	void sendDir(const string dirName, Buffer* sendBuf, int cfd);

	inline void setMethod(string method)
	{
		m_method = method;
	}
	inline void setUrl(string url)
	{
		m_url = url;
	}
	inline void setVersion(string version)
	{
		m_version = version;
	}


private:
	char* splitRequestLine(const char* start, const char* end, const char* sub, function<void(string)> callback);
	int hexToDec(char c);

private:
	string m_method;
	string m_url;
	string m_version;
	map<string, string> m_reqHeaders;
	processState m_curState;
};



