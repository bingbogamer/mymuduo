#ifndef MUDUO_NET_EVENTLOOPTHREADPOOL_H
#define MUDUO_NET_EVENTLOOPTHREADPOOL_H

#include "noncopyable.h"
#include <functional>
#include <string>
#include <memory>
#include <vector>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
public:
    // ThreadInitCallback  等价于   function<void(EventLoop *)>
    typedef std::function<void(EventLoop *)> ThreadInitCallback;
    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    // 设置线程数量
    void setThreadNum(int numThreads) { numThreads_ = numThreads; }
    void start(const ThreadInitCallback &cb = ThreadInitCallback());
    
    // valid after calling start()
    // round-robin 轮询
    EventLoop *getNextLoop(); // 如果工作在多线程中，baseLoop_默认以轮询的方式  分配channel 给subloop

    std::vector<EventLoop *> getAllLoops();
    bool started() const { return started_; }
    const std::string &name() const { return name_; }

private:
    EventLoop *baseLoop_; // baseLoop: 用户创建的 mainloop
    std::string name_;    // nameArg: 用户设置的 服务器名
    bool started_;
    int numThreads_;        // numThreads_：用户设置的 线程数量
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop *> loops_;    // 存储所有的 EventLoop，包括mainloop
};

#endif // MUDUO_NET_EVENTLOOPTHREADPOOL_H