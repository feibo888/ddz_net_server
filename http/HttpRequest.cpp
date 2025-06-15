#include "HttpRequest.h"


// 将字符转换为整形数
int HttpRequest::hexToDec(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return 0;
}



HttpRequest::HttpRequest()
{
	reset();
}

HttpRequest::~HttpRequest()
{
}

void HttpRequest::reset()
{
	m_curState = processState::ParseReqLine;
	m_method = m_url = m_version = string();
	m_reqHeaders.clear();
}


void HttpRequest::addHeader(const string key, const string value)
{
	if (key.empty() || value.empty())
	{
		return;
	}
	m_reqHeaders.insert(make_pair(key, value));
}

string HttpRequest::getHeader(const string key)
{
	auto item = m_reqHeaders.find(key);
	if (item == m_reqHeaders.end())
	{
		return string();
	}
	return m_reqHeaders[key];
}

bool HttpRequest::parseRequestLine(Buffer* readbuf)
{
	char* end = readbuf->findCRLF();
	char* start = readbuf->data();

	int lineSize = end - start;

	if (lineSize)
	{
		auto methodFunc = bind(&HttpRequest::setMethod, this, placeholders::_1);
		start = splitRequestLine(start, end, " ", methodFunc);

		auto urlFunc = bind(&HttpRequest::setUrl, this, placeholders::_1);
		start = splitRequestLine(start, end, " ", urlFunc);

		auto versionFunc = bind(&HttpRequest::setVersion, this, placeholders::_1);
		splitRequestLine(start, end, nullptr, versionFunc);


		//为解析请求头做准备
		readbuf->readPosIncrease(lineSize + 2);

		//修改状态
		setStatus(processState::ParseReqHeaders);
		return true;
	}

	return false;
}

bool HttpRequest::parseRequestHeader(Buffer* readbuf)
{
	char* end = readbuf->findCRLF();
	if (end != nullptr)
	{
		char* start = readbuf->data();
		int lineSize = end - start;

		char* middle = static_cast<char*>(memmem(start, lineSize, ": ", 2));
		if (middle != nullptr)
		{
			int keyLen = middle - start;
			int valueLen = end - middle - 2;
			if (keyLen > 0 && valueLen > 0)
			{
				string key(start, keyLen);
				string value(middle + 2, valueLen);
				addHeader(key, value);
			}
			//移动读数据的位置
			readbuf->readPosIncrease(lineSize + 2);
		}
		else
		{
			readbuf->readPosIncrease(2);
			//修改解析状态，这里不考虑post
			setStatus(processState::ParseReqDone);
		}
		return true;
	}

	return false;
}

bool HttpRequest::parseHttpRequest(Buffer* readbuf, HttpResponse* response, Buffer* sendBuf, int socket)
{
	bool flag = true;
	while (m_curState != processState::ParseReqDone)
	{
		switch (m_curState)
		{
		case processState::ParseReqLine:
			flag = parseRequestLine(readbuf);
			break;
		case processState::ParseReqHeaders:
			flag = parseRequestHeader(readbuf);
		case processState::ParseReqBody:
			break;
		default:
			break;
		}
		if (!flag)
		{
			return flag;
		}
		if (m_curState == processState::ParseReqDone)
		{
			//1. 根据解析出的原始数据，对客户端的请求做出处理
			processHttpRequest(response);
			//2. 组织响应数据并发送给客户端
			response->prepareMsg(sendBuf, socket);
		}
	}
	m_curState = processState::ParseReqLine;
	return flag;
}

bool HttpRequest::processHttpRequest(HttpResponse* response)
{
	if (strcasecmp(m_method.data(), "get") != 0)
	{
		return false;
	}

	//cout << m_url << endl;
	m_url = decodeMsg(m_url);
	//cout << m_url << endl;

	//处理客户端请求的静态资源
	const char* file = NULL;
	if (strcmp(m_url.data(), "/") == 0)
	{
		file = "./";
	}
	else
	{
		file = m_url.data() + 1;
	}

	//获取文件属性
	struct stat st;
	int ret = stat(file, &st);
	if (ret == -1)
	{
		//文件不存在，回复404
		response->setStatusCode(HttpStatusCode::NotFound);
		response->setFileName("404.html");
		response->addHeader("Content-type", getFileType(".html"));
		auto it = bind(&HttpRequest::sendFile, this, placeholders::_1, placeholders::_2, placeholders::_3);
		response->m_sendDataFunc = it;

		return false;
	}

	response->setStatusCode(HttpStatusCode::OK);
	response->setFileName(file);

	if (S_ISDIR(st.st_mode))
	{
		//目录处理，发目录下的资源
		//响应头
		response->addHeader("Content-type", getFileType(".html"));
		auto it = bind(&HttpRequest::sendDir, this, placeholders::_1, placeholders::_2, placeholders::_3);
		response->m_sendDataFunc = it;
	}
	else
	{
		//文件处理，发文件
		//响应头
		response->addHeader("Content-type", getFileType(file));
		response->addHeader("Content-length", to_string(st.st_size));
		auto it = bind(&HttpRequest::sendFile, this, placeholders::_1, placeholders::_2, placeholders::_3);
		response->m_sendDataFunc = it;
	}
	return true;
}


string HttpRequest::decodeMsg(string msg)
{
	string str = string();
	const char* from = msg.data();
	for (; *from != '\0'; ++from)
	{
		// isxdigit -> 判断字符是不是16进制格式, 取值在 0-f
		// Linux%E5%86%85%E6%A0%B8.jpg
		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
		{
			// 将16进制的数 -> 十进制 将这个数值赋值给了字符 int -> char
			// B2 == 178
			// 将3个字符, 变成了一个字符, 这个字符就是原始数据
			str.append(1, hexToDec(from[1]) * 16 + hexToDec(from[2]));

			// 跳过 from[1] 和 from[2] 因此在当前循环中已经处理过了
			from += 2;
		}
		else
		{
			// 字符拷贝, 赋值
			str.append(1, *from);
		}
	}
	//str.append(1, '\0');
	return str;
}

const string HttpRequest::getFileType(const string name)
{
	// a.jpg a.mp4 a.html
	// 自右向左查找‘.’字符, 如不存在返回NULL
	const char* dot = strrchr(name.data(), '.');
	if (dot == NULL)
		return "text/plain; charset=utf-8";	// 纯文本
	if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
		return "text/html; charset=utf-8";
	if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
		return "image/jpeg";
	if (strcmp(dot, ".gif") == 0)
		return "image/gif";
	if (strcmp(dot, ".png") == 0)
		return "image/png";
	if (strcmp(dot, ".css") == 0)
		return "text/css";
	if (strcmp(dot, ".au") == 0)
		return "audio/basic";
	if (strcmp(dot, ".wav") == 0)
		return "audio/wav";
	if (strcmp(dot, ".avi") == 0)
		return "video/x-msvideo";
	if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
		return "video/quicktime";
	if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
		return "video/mpeg";
	if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
		return "model/vrml";
	if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
		return "audio/midi";
	if (strcmp(dot, ".mp3") == 0)
		return "audio/mpeg";
	if (strcmp(dot, ".ogg") == 0)
		return "application/ogg";
	if (strcmp(dot, ".pac") == 0)
		return "application/x-ns-proxy-autoconfig";

	return "text/plain; charset=utf-8";
}

void HttpRequest::sendFile(const string fileName, Buffer* sendBuf, int cfd)
{
	//打开文件
	int fd = open(fileName.data(), O_RDONLY);
	assert(fd > 0);

#if 1
	while (1)
	{
		char buf[1024];
		int len = read(fd, buf, sizeof(buf));
		if (len > 0)
		{
			//send(cfd, buf, len, 0);
			sendBuf->appendString(buf, len);

#ifndef _SEND_MSG_AUTO
			sendBuf->sendData(cfd);
#endif // !_SEND_MSG_AUTO
		}
		else if (len == 0)
		{
			break;
		}
		else
		{
			perror("read");
		}
	}
#else
	//零拷贝发文件
	int size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	off_t offset = 0;

	int ret = 0;
	while (offset < size)
	{
		ret = sendfile(cfd, fd, &offset, size - offset);
		/*if (ret == -1)
		{
			perror("sendfile");
		}*/
		if (ret == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				// 发送缓冲区已满，等待一小段时间再重试
				usleep(10000);  // 等待10毫秒
				continue;
			}
			else
			{
				// 其他错误，退出循环
				perror("sendfile");
				break;
			}
		}
	}

#endif
	close(fd);
}

void HttpRequest::sendDir(const string dirName, Buffer* sendBuf, int cfd)
{
	char buf[4096] = { 0 };
	//sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirName);
	sprintf(buf,
		"<html>\n"
		"    <head>\n"
		"        <meta charset=\"UTF-8\">\n"
		"        <title>%s</title>\n"
		"    </head>\n"
		"    <body>\n"
		"        <table>\n",
		dirName.data()
	);

	struct dirent** nameList;
	int num = scandir(dirName.data(), &nameList, NULL, alphasort);
	for (int i = 0; i < num; ++i)
	{
		char* name = nameList[i]->d_name;
		struct stat st;
		char subPath[1024] = { 0 };
		sprintf(subPath, "%s/%s", dirName.data(), name);
		stat(subPath, &st);
		if (S_ISDIR(st.st_mode))
		{
			// a标签 <a href="">name</a>
			/*sprintf(buf + strlen(buf),
				"<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
				name, name, st.st_size);*/
			sprintf(buf + strlen(buf),
				"            <tr>\n"
				"                <td><a href=\"%s/\">%s</a></td>\n"
				"                <td>%ld</td>\n"
				"            </tr>\n",
				name, name, st.st_size
			);
		}
		else
		{
			/*sprintf(buf + strlen(buf),
				"<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
				name, name, st.st_size);*/
			sprintf(buf + strlen(buf),
				"            <tr>\n"
				"                <td><a href=\"%s\">%s</a></td>\n"
				"                <td>%ld</td>\n"
				"            </tr>\n",
				name, name, st.st_size
			);
		}

		sendBuf->appendString(buf);

#ifndef _SEND_MSG_AUTO
		sendBuf->sendData(cfd);
#endif // !_SEND_MSG_AUTO

		memset(buf, 0, sizeof(buf));
		free(nameList[i]);
	}
	//sprintf(buf, "</table></body></html>");
	sprintf(buf,
		"        </table>\n"
		"    </body>\n"
		"</html>\n"
	);
	//send(cfd, buf, strlen(buf), 0);
	sendBuf->appendString(buf);

#ifndef _SEND_MSG_AUTO
	sendBuf->sendData(cfd);
#endif // !_SEND_MSG_AUTO

	free(nameList);
}

char* HttpRequest::splitRequestLine(const char* start, const char* end, const char* sub, function<void(string)> callback)
{
	char* space = const_cast<char*>(end);
	if (sub != NULL)
	{
		space = static_cast<char*>(memmem(start, end - start, sub, strlen(sub)));
		assert(space != NULL);
	}
	int length = space - start;

	callback(string(start, length));
	return space + 1;
}
