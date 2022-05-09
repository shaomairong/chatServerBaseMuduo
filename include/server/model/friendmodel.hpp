#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H

#include "user.hpp"
#include <vector>
using namespace std;

// 维护好友信息的操作接口方法
class FriendModel
{
public: 
    // 添加好友关系:将(userid,friend)插入数据库表中
    void insert(int userid, int friendid);
    //不提供删除好友功能(业务功能就不说了，主要关注muduo网络库的使用)

    // 返回用户好友列表 :用户登陆成功后要返回它的好友列表(一把来说好友列表很多，都是记录在客户端的;
    //如果每次用户登录都从服务器返回好友列表，那么服务器压力就很大了)
    vector<User> query(int userid);//这里简单起见，就不再客户端存放好友列表信息了
    //这里需要通过用户id找到所有好友列表-id ;然后又需要再user表中通过id查到用户名字返回，所以这里使用二个表的联合查询



};

#endif