#ifndef MUDUO_NET_EVENTLOOPTHREAD_H
#define MUDUO_NET_EVENTLOOPTHREAD_H

#include <functional>
#include <mutex>
#include <condition_variable> // C++ std标准库
#include <string>

#include "noncopyable.h"
#include "Thread.h"

class EventLoop;

// 绑定Loop和Thread，让Loop运行这个Thread上
// one loop per thread
class EventLoopThread : noncopyable
{
public:
    typedef std::function<void(EventLoop *)> ThreadInitCallback; // 返回值void  形参：EventLoop*
    // ThreadInitCallback()  : 默认构造创建一个空function 调用包装器。
    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(), const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop *startLoop();

private:
    void threadFunc(); // 线程函数

    EventLoop *loop_;
    Thread thread_;
    
    bool exiting_;
    
    std::mutex mutex_;
    std::condition_variable cond_; // 条件变量
    ThreadInitCallback callback_;  // 回调函数
};

#endif // !MUDUO_NET_EVENTLOOPTHREAD_H