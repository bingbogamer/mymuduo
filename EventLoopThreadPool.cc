#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"

// 在TcpServer构造函数  初始化列表中  new了一个 的 EventLoopThreadPool对象
// baseLoop: 用户创建的 mainloop
// nameArg: 用户设置的 服务器名: EchoServer-01
EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop),
      name_(nameArg),   // EchoServer-01
      started_(false),
      numThreads_(0),
      next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
    // Don't delete loop, it's stack variable
    // 线程绑定的 loop 是栈上的对象，不用delete 释放
}

// 在 TcpServer::start()中 被调用
// void start(const ThreadInitCallback &cb = ThreadInitCallback());
// cb: TcpServer 的threadInitCallback_传入
void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;
    // numThreads_ > 0 时，会进入循环
    for (int i = 0; i < numThreads_; ++i)
    {
        char buf[name_.size() + 32];       // EchoServer-01 + 32个字符
        // 格式化输出字符串，并将结果写入到指定的缓冲区
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i); // EchoServer-01 + 线程编号
        // EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(), const string &name = std::string())
        // 实例化一个EventLoopThread对象
        EventLoopThread *thread = new EventLoopThread(cb, buf); 
        // vector<std::unique_ptr<EventLoopThread>> threads_;
        threads_.push_back(std::unique_ptr<EventLoopThread>(thread)); // 
        
        // 底层创建线程，绑定一个新的EventLoop，并返回该 loop的地址
        loops_.push_back(thread->startLoop());
    }
    // 整个服务器 只有一个线程，运行着 baseloop
    if (numThreads_ == 0 && cb)
        cb(baseLoop_);
}



// valid after calling start()
// round-robin 轮询
EventLoop *EventLoopThreadPool::getNextLoop()
{
    EventLoop *loop = baseLoop_;    // 定义一个局部指针 loop 指向 传入的baseLoop_

    // std::vector<EventLoop *> loops_;   EventLoop指针数组
    if (!loops_.empty()) // 通过轮询，获取下一个处理事件的loop
    {
        // round-robin
        loop = loops_[next_];
        ++next_;
        // 
        if (next_ >= loops_.size())
            next_ = 0;
    }
    return loop;
}

std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty())
        return std::vector<EventLoop *>(1, baseLoop_);
    else
        return loops_;
}
