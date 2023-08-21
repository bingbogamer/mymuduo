#ifndef MUDUO_NET_EVENTLOOP_H
#define MUDUO_NET_EVENTLOOP_H

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

class Channel;
class Poller;

// 事件循环类， 主要包含了两大模块： Channel 和  Polloer

// 反应器，每个线程最多一个。
// 这是一个接口类，所以不要暴露太多细节。
class EventLoop : noncopyable
{
public:
    typedef std::function<void()> Functor;
    EventLoop(); // 默认构造函数
    ~EventLoop();

    void loop(); // 开启事件循环
    void quit(); // 退出事件循环

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    void runInLoop(Functor cb);   // 在当前Loop中执行
    void queueInLoop(Functor cb); // 把cb放入队列中，唤醒loop所在的线程，执行cb

    void wakeup(); // mainReactor 唤醒 loop所在的线程 subReactor

    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);
    // 判断EventLoop对象是否在自己的线程里面
    // threadId_ ：保存创建当前EventLoop对象的线程ID
    // CurrentThread::tid(): 调用当前方法的线程ID
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); } 

private:
    void handleRead();        // wake up
    void doPendingFunctors(); // 执行回调

    std::atomic<bool> looping_; // 事件循环是否正在进行
    std::atomic<bool> quit_;    // 标识退出loop循环

    const pid_t threadId_; // 记录当前loop所在的线程id

    Timestamp pollReturnTime_;       // Poller返回发生事件的 channels的时间点
    std::unique_ptr<Poller> poller_; // “指向派生类的基类指针” 访问虚函数，访问的是派生类中的实现

    // int eventfd(usigned int initval, int flags);     创建一个文件描述符 用于 事件通知
    // 主要作用： 当mianloop获取一个新用户的channel，通过轮询算法选择一个subloop，通过wakeupFd_唤醒subloop处理Channel
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    typedef std::vector<Channel *> ChannelList;
    ChannelList activeChannels_;                // 数组：储存 Channel * 指针
    Channel *currentActivaChannel_;

    std::atomic<bool> callingPendingFunctors_; // 标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_;     // 存储 loop
    std::mutex mutex_;                         // 互斥锁，保护上面vector容器的 线程安全操作
};

#endif // MUDUO_NET_EVENTLOOP_H