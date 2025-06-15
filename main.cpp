#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "TcpServer.h"
#include "Mytest.h"

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		printf("./a.out port");
		return -1;
	}

	unsigned short port = atoi(argv[1]);

	TcpServer* server = new TcpServer(port, 8);
	server->run();

	// MyTest* test = new MyTest();
	// test->test();

	return 0;
}