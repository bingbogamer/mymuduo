#ifndef MUDUO_NET_POLLER_EPOLLPOLLER_H
#define MUDUO_NET_POLLER_EPOLLPOLLER_H

#include "Poller.h"
#include <vector>
#include <sys/epoll.h>

// 继承自 抽象基类Poller
/**
 * 封装了epoll
*/
class EPollPoller : public Poller
{
public:
    // epoll_create()
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;    // 覆盖基类的 虚析构函数

    // 轮询I/O事件  
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;    // epoll_wait()
    // 更改感兴趣的I/O事件。
    void updateChannel(Channel *channel) override;      // epoll_ctl()
    // 当Channel销毁时，删除通道。
    void removeChannel(Channel *channel) override;      // epoll_ctl()
    
private:
    // 填写 活跃的链接
    void fillActiveChannels(int numsEvents, ChannelList *activeChannels) const;
    // 更新channel通道
    void update(int operation, Channel *channel);      // epoll_ctl()

    static const int kInitEventListSize = 16;   // 静态数据成员（已经初始化，类外最好再定义一下）
    typedef std::vector<epoll_event> EventList;
    EventList events_;
    
    int epollfd_;       // ::epoll_create1(EPOLL_CLOEXEC)返回的 efd
};

#endif // !1