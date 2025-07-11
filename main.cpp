#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "TcpServer.h"
#include "MyTest.h"
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <signal.h>

#include <glog/logging.h>

// 全局变量，用于信号处理
TcpServer* g_server = nullptr;

// 信号处理函数
void signalHandler(int signal) {
    if (signal == SIGINT) {
        LOG(INFO) << "收到 SIGINT 信号，开始清理资源...";

    	// 更彻底的 OpenSSL 清理（适用于 OpenSSL 3.x）
    	OPENSSL_thread_stop();  // 清理线程局部存储
    	ERR_free_strings();
    	EVP_cleanup();
    	CRYPTO_cleanup_all_ex_data();

		// 关闭 glog
        google::ShutdownGoogleLogging();
    	if (g_server) {
    		delete g_server;
    		g_server = nullptr;
    	}

        LOG(INFO) << "资源清理完成，程序退出";
        exit(0);
    }
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		printf("./a.out port");
		return -1;
	}

	google::InitGoogleLogging(argv[0]);		//使用glog之前必须先初始化库，仅需执行一次，括号内为程序名
	FLAGS_colorlogtostderr = true;			//是否启用不同颜色显示
	FLAGS_minloglevel = google::INFO;

	google::SetLogDestination(google::GLOG_INFO, "../log/INFO.log");//INFO级别的日志都存放到logs目录下且前缀为INFO_
	google::SetLogDestination(google::GLOG_WARNING, "../log/WARNING.log");//WARNING级别的日志都存放到logs目录下且前缀为WARNING_
	google::SetLogDestination(google::GLOG_ERROR, "../log/ERROR.log");	//ERROR级别的日志都存放到logs目录下且前缀为ERROR_
	google::SetLogDestination(google::GLOG_FATAL, "../log/FATAL.log");	//FATAL级别的日志都存放到logs目录下且前缀为FATAL_

	//FLAGS_logtostderr = true;  //设置日志消息是否转到标准输出而不是日志文件

	//FLAGS_alsologtostderr = true;  //设置日志消息除了日志文件之外是否去标准输出

	//FLAGS_colorlogtostderr = true;  //设置记录到标准输出的颜色消息（如果终端支持）

	//FLAGS_log_prefix = true;  //设置日志前缀是否应该添加到每行输出

	FLAGS_logbufsecs = 5;  //设置可以缓冲日志的最大秒数，0指实时输出

	FLAGS_max_log_size = 10;  //设置最大日志文件大小（以MB为单位）

	FLAGS_stop_logging_if_full_disk = true;  //设置是否在磁盘已满时避免日志记录到磁盘

	// 注册信号处理器
	signal(SIGINT, signalHandler);

	unsigned short port = atoi(argv[1]);

	TcpServer* server = new TcpServer(port, 8);
	g_server = server;  // 保存到全局变量
	server->run();

	// 这些代码在正常情况下不会执行到，但保留以防万一
	// 清理 OpenSSL 资源
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();

	google::ShutdownGoogleLogging();

	return 0;
}