#ifndef REDIS_H
#define REDIS_H

#include <hiredis/hiredis.h>
#include <thread>
#include <functional>
using namespace std;

/*
redis作为集群服务器通信的基于发布-订阅消息队列时，会遇到两个难搞的bug问题，参考我的博客详细描述：
https://blog.csdn.net/QIANGWEIYUAN/article/details/97895611
*/
class Redis
{
public:
    Redis(); //构造函数中将二个上下文成员指针设置为空
    ~Redis();//析构函数中需要释放上下文资源()

    // 连接redis服务器 
    bool connect();

    // 向redis指定的通道channel发布消息
    bool publish(int channel, string message);

    // 向redis指定的通道subscribe订阅消息
    bool subscribe(int channel);

    // 向redis指定的通道unsubscribe取消订阅消息
    bool unsubscribe(int channel);//如果用户下线了，那就需要取消订阅;它下线了，那么发给他的就是离线消息，直接传到数据库

    // 在独立线程中接收订阅通道中的消息
    void observer_channel_message();

    // 初始化向业务层上报通道消息的回调对象
    void init_notify_handler(function<void(int, string)> fn);
    //redis监测到相应的通道有消息发生就会给chatserver通知，所以这里需要先注册事件后面才能通知;

private:
    /*因为一个上下文如果subscribe的话，这个上下文就被阻塞住了;
    所以subscribe和publish这二个命令是不能够在一个上下文处理的，必须得有二个上下文;*/

    // hiredis同步上下文对象，负责publish消息
    redisContext *_publish_context;

    // hiredis同步上下文对象，负责subscribe消息
    redisContext *_subcribe_context; //订阅消息后当前上下文就阻塞住了

    // 回调操作，收到订阅的消息，给service层上报
    function<void(int, string)> _notify_message_handler;
};

#endif
