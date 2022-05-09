#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h> //muduo的日志
#include <vector>
using namespace std;
using namespace muduo;

// 获取单例对象的接口函数
ChatService *ChatService::instance() //静态函数在类外定义不需要写static了
{
    static ChatService service; //局部静态对象，在第一次执行时初始化，后面都不会被初始化了；
    return &service;
}

//构造函数： 注册消息以及对应的Handler回调操作，即对map操作
ChatService::ChatService()
{
    //下面就是网络模块和业务模块进行解耦的核心

    // 用户基本业务管理相关事件处理回调注册
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});

    // 群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接redis服务器,连接成功后，设置redis上报消息的回调函数
    if (_redis.connect())
    {
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

// 服务器异常，业务重置方法
void ChatService::reset()
{
    // 把online状态的用户，设置成offline
    _userModel.resetState();
}

// 获取消息对应的处理器：即从map中返回msgid对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end()) //没找到
    {
        // 返回一个默认的处理器，空操作 (lambda表达式)
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp)
        {
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
            //使用muduo的日志模块打印错误信息
        };
    }
    else //找到了
    {
        return _msgHandlerMap[msgid];
    }
}

// 处理登录业务:输入id和pwd，检测检测id对应的密码是不是正确的
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"].get<int>(); // js对象转成int
    string pwd = js["password"];

    User user = _userModel.query(id); //通过id查询对应的用户
    if (user.getId() == id && user.getPwd() == pwd)
    {
        if (user.getState() == "online")
        {
            // 该用户已经登录，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "this account is using, input another!";
            conn->send(response.dump());
        }
        else
        {
            // 登录成功，记录用户连接信息，后面就可以从id知道连接conn了: 给useCommMap进行insert要进行操作要加锁
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
                //用{}减小锁的粒度
            }
            //注：数据库的并发操作是由MySQL server保证的

            // id用户登录成功后，向redis订阅channel(id)
            _redis.subscribe(id);

            // 登录成功，更新用户状态信息 state offline=>online
            user.setState("online");      //对user设置用户状态信息
            _userModel.updateState(user); //通过userModel的updateState方法更新用户状态信息

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 查询当前用户id是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if (!vec.empty()) //不空
            {
                response["offlinemsg"] = vec;
                // 读取该用户的离线消息后，把该用户的所有离线消息删除掉
                _offlineMsgModel.remove(id);
            }

            // 登录成功时候要查询该用户的好友信息并返回
            vector<User> userVec = _friendModel.query(id);//通过用户id查询用户的好友信息
            if (!userVec.empty())//非空表示有好友就要返回
            {
                vector<string> vec2;
                for (User &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;//把包含好友信息的vector放入response["friends"]中
            }

            // 查询用户的群组信息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                // group:[{groupid:[xxx, xxx, xxx, xxx]}]
                vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }

                response["groups"] = groupV;
            }

            conn->send(response.dump());
        }
    }
    else
    {
        // 该用户不存在 或者 用户存在但是密码错误，登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "id or password is invalid!";
        conn->send(response.dump());
    }
}

// 处理注册业务  name  password
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{ //当注册的的消息来了之后，网络模块将消息进行反序列化生成json数据对象上报到注册业务这里
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    //调用userMode的inert方法进行新用户的插入，user是引用传递，id会被设置；
    if (state)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK; // msgid为REG_MSG_ACK表示是注册响应消息
        response["errno"] = 0;           // errno为0表示注册的成功了
        response["id"] = user.getId();
        conn->send(response.dump()); //给对端响应注册结果
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1; // errno为1表示注册的失败了
        conn->send(response.dump());
    }
}

// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();

    {//服务器中的用户连接队列删除当前用户的连接
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }

    // 用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);

    // 更新用户的状态信息
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex); //要修改_userConnMap，所以需要加锁
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        //因为只有value（conn），所以必须遍历整个map然后删除用户连接信息
        {
            if (it->second == conn)
            {
                // 从map表删除用户的链接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    // 用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(user.getId());

    // 更新用户的状态信息:这里不需要提供线程安全，因为是MySQL server提供保证的
    if (user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user); //数据库操作
    }
}

// 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["toid"].get<int>(); //获取toid

    { //要访问连接信息表了，看看toid用户是不是在线
        lock_guard<mutex> lock(_connMutex); //保证线程安全
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            // toid在线:[转发消息]   服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
    }
    //用户不存在可能是在当前服务器不在线，集群环境下可能在其他服务器上在线，所以这里要处理：
    User user = _userModel.query(toid);
    if (user.getState() == "online")
    //如果数据库表中显示用户online，说明用户在其他服务器上登录，那么这里将消息发送给redis的toid通道上;
    //这样原来注册过toid的通道就会有事件发生了,就可以获取到消息了
    {
        _redis.publish(toid, js.dump());
        return;
    }

    // toid不在线，存储离线消息
    _offlineMsgModel.insert(toid, js.dump());
}

// 添加好友业务 msgid id friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息:写入friend表中
    _friendModel.insert(userid, friendid);
    /*在这也可以验证frientid是不是存在，也可以*/
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1, name, desc);
    if (_groupModel.createGroup(group)) //allGroup表
    {
        // 如果群组创建成功，存储群组创建人信息
        _groupModel.addGroup(userid, group.getId(), "creator");//groupUser表
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    //查询当前用户所在群组中的其他用户id
    lock_guard<mutex> lock(_connMutex); //操作userConmap需要加锁，保证线程安全
    for (int id : useridVec) 
    {
        auto it = _userConnMap.find(id); 
        if (it != _userConnMap.end())
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else
        {
            // 查询toid是否在线
            User user = _userModel.query(id);
            if (user.getState() == "online")
            {
                _redis.publish(id, js.dump());
            }
            else
            {
                // 存储离线群消息
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}

// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    //因为有可能出现之前往redis通道写消息时在线，当现在从redis通道取消息处理时候不在线了，所以需要再次判断是否在线
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())//如果用户在线，直接发送消息给它
    {
        it->second->send(msg);
        return;
    }
    // 用户不在线，存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}