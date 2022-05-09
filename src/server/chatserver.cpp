#include "chatserver.hpp"
#include "json.hpp"
#include "chatservice.hpp"

#include <iostream>
#include <functional>
#include <string>
using namespace std;
using namespace placeholders;
using json = nlohmann::json;

// 初始化聊天服务器对象
ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg)
    : _server(loop, listenAddr, nameArg), _loop(loop)
{
    // 注册链接回调
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));

    // 注册消息回调
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    // 设置线程数量
    _server.setThreadNum(4);//线程数量一般是根据CPU核心设置，这里一个IO线程，三个worker线程
    //主reactor负责新用户的连接，子reeactor负责已连接用户的读写事件处理

}

// 启动服务
void ChatServer::start()
{
    _server.start();
}

// 上报链接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    // 客户端断开链接
    if (!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn); 
        conn->shutdown(); //客户端断开连接了，服务端释放socket资源
    }
}

// 上报读写事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buffer,
                           Timestamp time)
{
    string buf = buffer->retrieveAllAsString(); //把缓冲区buffer里面的数据转成字符串

    // 测试，添加json打印代码
    cout << buf << endl;

    // 数据的反序列化（把字符串string转成json对象）
    json js = json::parse(buf);
    // 达到的目的：<完全解耦网络模块的代码和业务模块的代码>
    //通过读取到的js["msgid"],实现对不同的msgid绑定不同的回调操作，不管网络做了什么，只要解析到msgid就可以获取到业务处理器handler
    //给业务处理函数三个参数:TcpConnectionPat& conn , json &js , Timestamp time;
    // auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    auto chatService = ChatService::instance(); //获取CHatService的唯一实例(单例模式提供的获取实例的静态函数instance)
    auto msgHandler = chatService->getHandler(js["msgid"].get<int>());
    //js["msgid"]是json对象值，要转成int类型;用要转的类型实例化get方法，它底层会进行强转为指定的类型
    // 回调消息绑定好的事件处理器，来执行相应的业务处理
    msgHandler(conn, js, time);

    //上面三行代码就实现了网络模块和业务模块进行解耦了，通过msgid找到对应的处理器函数，然后调用;
    

}