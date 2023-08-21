#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"
#include <errno.h>
#include <unistd.h>
#include <memory.h>

const int kNew = -1;    // channel没有添加到EPoll
const int kAdded = 1;   // channel已经添加到Epoll
const int kDeleted = 2; // channel已经从EPoll删除

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop), // Poller基类没有定义默认构造函数，所以在派生类初始化列表中必须指明Poller的构造函数
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize)   // 默认长度16
{
    if (epollfd_ < 0)
        LOG_FATAL("epoll_create error:%d \n", errno);
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_); 
}

// 轮询I/O事件  ——  调用 epoll_wait()
// 监听ChannelMap channels_上注册的 所有fd 上的事件
// 将epoll_wait() 监听到的所有发生事件的channel 通过 activeChannels形参，返回告知给 EventLoop的 ChannelList
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // 实际上应该用LOG_DEBUG输出日志更为合理，避免频繁的IO操作
    LOG_INFO("func=%s => fd total count: %lu\n", __FUNCTION__, channels_.size());

    // epoll_wait (int __epfd, struct epoll_event *__events,int __maxevents, int __timeout)
    // 参数2： epoll_event *__events 传出参数[数组]  满足监听条件的所有fd结构体
    // 为了方便扩容，这里使用的是 vector<epoll_event> EventList events_;
    // events_.begin() : 指向vector数组首元素的迭代器 ，*it：首元素值，
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);

    // poll()在多个线程里都会被调用，因为每个线程对应1个EventLoop，再对应一个Poller
    // 所以多个线程的多个EventLoop，都可能去epoll_wait()出错，写errno
    int saveErrno = errno;           // 记录全局变量  errno
    Timestamp now(Timestamp::now()); // 构造一个now对象

    // 如果有事件发生
    if (numEvents > 0)
    {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents,   );
        // 判断是否需要扩容
        if (numEvents == events_.size())
            events_.resize(events_.size() * 2); // 将vector<epoll_event>扩容2倍
    }
    else if (numEvents == 0) // 没有事件发生
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    else
    {
        // 发生错误时，记录不常见的错误(中断系统调用)
        if (saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}

// 更改感兴趣的I/O事件。 —— 调用update的epoll_ctl()
/**
 *                  EventLoop
 *           ChannelList       Poller
 *                           ChannelMap <fd, channel*>
 */
void EPollPoller::updateChannel(Channel *channel) // 在Channel::update()中调用
{
    // Channel的int index_成员，初始化为-1，即kNew 表示channel没有添加到EPoll
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);
    // 当channel是新创建 || 已经删除
    if (index == kNew || index == kDeleted){
        if (index == kNew){
            int fd = channel->fd();
            channels_[fd] = channel; // 从Poller基类中继承的成员channels_
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel); // 将channel往此Epoll对象中注册
    }
    else{ // channel 已经注册在poller
        int fd = channel->fd();
        if (channel->isNoneEvent()){
            update(EPOLL_CTL_DEL, channel); //
            channel->set_index(kDeleted);
        }
        else
            update(EPOLL_CTL_MOD, channel);
    }
}

// 当Channel销毁时，删除channel —— 调用update的epoll_ctl()
// 从poller中删除channel
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    // unordered_map<int, Channel *> ChannelMap
    channels_.erase(fd); // 将当前channel对应的fd，作为key, 从容器对象ChannelMap channels_中删除
    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, channel->fd());

    int index = channel->index();
    if (index == kAdded)
        update(EPOLL_CTL_DEL, channel);

    channel->set_index(kNew); // 修改为kNew
}

// vector<Channel *> ChannelList
void EPollPoller::fillActiveChannels(int numsEvents, ChannelList *activeChannels) const 
{
    // 遍历vector<epoll_event> EventList events_; 
    for (int i = 0; i < numsEvents; i++)
    {
        Channel * channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);     // EventLoop拿到了它的poller给他返回的所有发生事件的channel列表
    }
}

// 更新channel     调用epoll_ctl()
// operation:   EPOLL_CTL_ ADD\DEL\MOD
void EPollPoller::update(int operation, Channel *channel)
{
    // 初始化一个 epoll_event 对象
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    // int fd = channel->fd();
    // event.data.fd = fd;
    event.events = channel->events();
    event.data.ptr = channel; // void *指针ptr 指向 channel对象

    int fd = channel->fd();
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL) // DEL失败
            LOG_ERROR("epoll_ctl del error:%d \n", errno);
        else // ADD\MOD 失败，比较严重
            LOG_FATAL("epoll_ctl add/mod error:%d \n", errno);
    }
}