#ifndef MUDUO_BASE_THREAD_H
#define MUDUO_BASE_THREAD_H

#include "noncopyable.h"
#include <functional>
#include <thread> // C++线程类
#include <memory>
#include <unistd.h>
#include <atomic>

// 
class Thread : noncopyable
{
public:
    typedef std::function<void()> ThreadFunc;
    // 防止隐式转换   静止拷贝初始化
    explicit Thread(ThreadFunc, const std::string &name = std::string());
    ~Thread();

    void start();
    void join();

    // 获取成员状态 接口
    bool started() const { return started_; }
    pid_t tid() const { return tid_; }
    const std::string &name() const { return name_; }
    static int numCreated() { return numCreated_; }

private:
    void setDefaultName();
    
    bool started_;      // 在start() -> true
    bool joined_;       // 在join() -> true 当前线程等待其他线程运行完，再向下运行

    // pthread_t  pthreadId_;
    // std::thread thread_;    // 线程会直接启动，定义一个智能指针来封装，自己来掌控线程对象产生的时机
    std::shared_ptr<std::thread> thread_;   
    pid_t tid_;                 // 记录创建线程的编号
    ThreadFunc func_;
    std::string name_;          // 线程对象的名字 
    static std::atomic_int numCreated_;     // 静态数据成员，最好类外定义一下，相当于全局变量
};

#endif // !MUDUO_BASE_THREAD_H