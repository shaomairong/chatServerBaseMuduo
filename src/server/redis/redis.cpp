#include "redis.hpp"
#include <iostream>
using namespace std;

Redis::Redis()
    : _publish_context(nullptr), _subcribe_context(nullptr)
{
}

Redis::~Redis()
{
    //析构函数中要释放上下文的资源
    if (_publish_context != nullptr)
    {
        redisFree(_publish_context);
    }

    if (_subcribe_context != nullptr)
    {
        redisFree(_subcribe_context);
    }
}

bool Redis::connect()
{ 
    //相当于创建了二个连接，一个上下文就是一个连接
    // 负责publish发布消息的上下文连接
    _publish_context = redisConnect("127.0.0.1", 6379);
    if (nullptr == _publish_context)
    {
        cerr << "connect redis failed!" << endl;
        return false;
    }

    // 负责subscribe订阅消息的上下文连接
    _subcribe_context = redisConnect("127.0.0.1", 6379);
    if (nullptr == _subcribe_context)
    {
        cerr << "connect redis failed!" << endl;
        return false;
    }

    // 在单独的线程中，监听通道上的事件，有消息给业务层进行上报
    thread t([&]() {
        observer_channel_message();
    });
    t.detach();

    cout << "connect redis-server success!" << endl;

    return true;
}

// 向redis指定的通道channel发布消息
bool Redis::publish(int channel, string message)
{ /*redisCommand相当于就是敲命令:它调用redisAppendCommand把要发送的命令缓存到本地，然后调用redisBufferWrite把命令发送
到redisServer上，然后它还会调用redisGetReply来以阻塞的方式等待命令的执行结果;
PUBLISH命令不像SUBSCRIBE一样会阻塞，它一执行马上就会回复，不会阻塞当前线程;;*/
    redisReply *reply = (redisReply *)redisCommand(_publish_context, "PUBLISH %d %s", channel, message.c_str());
    if (nullptr == reply)
    {
        cerr << "publish command failed!" << endl;
        return false;
    }
    freeReplyObject(reply);//上面返回值是动态生成的结构体，记得释放
    return true;
}

// 向redis指定的通道subscribe订阅消息
bool Redis::subscribe(int channel)
{ /*这里没有使用上面函数调用PUBLISH的方式去调用SUBSCRIBE，原因是SUBSCRIBE命令
本身会造成线程阻塞等待通道里面发生消息，我们这里只做订阅通道，不接收通道消息;
所以单独调用二个命令执行订阅，这样就不会阻塞住;
    通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    只负责发送命令，不阻塞接收redis server响应消息，否则和notifyMsg线程抢占响应资源 */
    if (REDIS_ERR == redisAppendCommand(this->_subcribe_context, "SUBSCRIBE %d", channel))
    {
        cerr << "subscribe command failed!" << endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subcribe_context, &done))
        {
            cerr << "subscribe command failed!" << endl;
            return false;
        }
    }
    // redisGetReply

    return true;
}

// 向redis指定的通道unsubscribe取消订阅消息:同subscribe
bool Redis::unsubscribe(int channel)
{
    if (REDIS_ERR == redisAppendCommand(this->_subcribe_context, "UNSUBSCRIBE %d", channel))
    {
        cerr << "unsubscribe command failed!" << endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subcribe_context, &done))
        {
            cerr << "unsubscribe command failed!" << endl;
            return false;
        }
    }
    return true;
}

// 在独立线程中接收订阅通道中的消息
void Redis::observer_channel_message()
{
    redisReply *reply = nullptr;
    //以循环阻塞的方式在subscribe_context上下文中等待是否有消息
    while (REDIS_OK == redisGetReply(this->_subcribe_context, (void **)&reply))
    {
        // 订阅收到的消息是一个带三元素的数组，写在element数组里面,下标分别是0,1,2;
        if (reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {//说明有消息了,就给给业务层上报通道上发生的消息
            _notify_message_handler(atoi(reply->element[1]->str) , reply->element[2]->str);
            //                              通道号                消息
        }

        freeReplyObject(reply); //释放资源
    }

    cerr << ">>>>>>>>>>>>> observer_channel_message quit <<<<<<<<<<<<<" << endl;
}

void Redis::init_notify_handler(function<void(int,string)> fn)
{
    this->_notify_message_handler = fn;
}