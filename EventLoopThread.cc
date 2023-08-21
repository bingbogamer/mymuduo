#include "EventLoopThread.h"
#include "EventLoop.h"


// cb: TcpServer 的threadInitCallback_传入
// name: EchoServer-01 + 线程编号
EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr),
      exiting_(false),
      // Thread(ThreadFunc, const std::string &name = std::string());
      thread_(std::bind(&EventLoopThread::threadFunc, this), name), // 将线程函数 传递给 thread_对象
      mutex_(),
      cond_(),
      callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != NULL)
    {
        loop_->quit();
        thread_.join();
    }
}

// 在EventLoopThreadPool::start() 中调用，返回
EventLoop *EventLoopThread::startLoop()
{
    // 创建 底层的新线程，执行func_()，即下面的 threadFunc() 函数
    // threadFunc() ：执行loop.loop()，开启事件循环
    thread_.start(); 
    EventLoop *loop = NULL;

    // 如果本线程先拿到mutex_，则 子线程一直拿不到 mutex_，则 loop_ 一直为 NULL，就是一直
    {
        std::unique_lock<std::mutex> lock(mutex_);  // 自动加锁，解锁
        // while循环，处理惊群效应，只能让一个线程处理某个任务时，其它被唤醒的线程就需要继续 wait
        while (loop_ == NULL){
            // 阻塞当前线程，直到条件变量cond_被唤醒
            // 阻塞的同时，会立刻解锁，这样子线程就会拿到 mutex_
            cond_.wait(lock);      
            // 唤醒后，会继续加锁
        }
        loop = loop_;   // 加锁状态下，获取loop_并返回
    }
    return loop;
}

// 注意：是在新建的线程Thread thread_里运行的，
// 不再是当前线程（创建EventLoopThread对象所在的线程）
void EventLoopThread::threadFunc()
{
    // one loop per thread
    // 创建一个独立的 eventloop，和上面的线程一一对应
    EventLoop loop; 

    // 如果ThreadInitCallback callback_ 是 空function, 则不会进入执行
    if (callback_)
        callback_(&loop);
        
    {
        std::unique_lock<std::mutex> lock(mutex_); 
        loop_ = &loop;          // 上锁，确保loop_先拿到 loop的地址
        cond_.notify_one();     // 唤醒等待队列中的第一个线程；不存在锁争用，所以能够立即获得锁。其余的线程不会被唤醒
    }

    // EventLoop loop -> Poller.poll
    // 开启事件循环，while循环
    loop.loop();  
    // 退出事件循环后，
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}