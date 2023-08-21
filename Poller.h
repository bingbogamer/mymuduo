#ifndef MUDUO_NET_POLLER_H
#define MUDUO_NET_POLLER_H

#include "noncopyable.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

// Poller 是一个抽象基类（含 纯虚函数 = 0），不能创建抽象类的 对象
// 派生类 要覆盖Poller中的纯虚函数，才能创建 派生类对象，否则其还是基类对象

// IO多路复用的基类（抽象基类）
// 这个类不拥有Channel对象
class Poller : noncopyable
{
public:
    typedef std::vector<Channel *> ChannelList;
    Poller(EventLoop *loop);     // 构造函数
    virtual ~Poller() = default; // 虚析构函数

    // 轮询I/O事件  必须在循环线程中调用
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    // 更改感兴趣的I/O事件。必须在循环线程中调用
    virtual void updateChannel(Channel *channel) = 0;
    // 当Channel销毁时，删除通道。必须在循环线程中调用
    virtual void removeChannel(Channel *channel) = 0;

    // 判断 参数channel是否在当前Poller中
    bool hasChannel(Channel *channel) const;

    // EventLoop可以通过该接口 获取默认的IO复用的具体实现
    static Poller *newDefaultPoller(EventLoop *loop);       // 静态成员函数：不属于类对象

protected:
    // Map的Key：sockfd     Value:sockfd所属的Channel通道类型
    typedef std::unordered_map<int, Channel *> ChannelMap;
    ChannelMap channels_;

private:
    EventLoop *ownerLoop_; // 定义Poller所属的 事件循环EventLoop
};

#endif