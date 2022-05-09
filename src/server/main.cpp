#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <signal.h>
using namespace std;

// 处理服务器ctrl+c结束后，重置user的状态信息
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc, char **argv) //muduo库是基于事件驱动的IO复用+线程池的网络库 (基于reactor模型)
{
    if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatServer 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    signal(SIGINT, resetHandler);//服务器收到ISGINT信息后，调用restHadnler函数

    EventLoop loop;//定义事件循环
    InetAddress addr(ip, port);//定义地址信息变量
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();//开启事件循环 

    return 0;
}