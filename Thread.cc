#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h> // 该信号量库是linux系统库，影响跨平台

// 静态数据成员  必须在类外初始化
std::atomic_int Thread::numCreated_(0); // 相当于全局变量

// 传入的线程执行函数
Thread::Thread(ThreadFunc func, const std::string &name)    // 默认 name = std::string()
    : started_(false), 
    joined_(false), 
    tid_(0), 
    func_(std::move(func)), 
    name_(name)
{
    setDefaultName();   // 如果没有传入name，即name是空string，则赋值为 "Thread num"
}

// 创建新线程后，如果没有设置回收该新线程，则将该线程设置分离，运行完毕自动回收
Thread::~Thread()
{
    if (started_ && !joined_)    
        thread_->detach(); 
}

// 启动一个线程 —— 之前都没有创建线程，这里才是创建了线程对象
void Thread::start()
{
    started_ = true;
    sem_t sem;            // 定义信号量
    sem_init(&sem, 0, 0); // 0: 线程之间共享   0: 信号量初值

    // new std::thread() —— 创建新线程，该线程执行 
    // 智能指针thread_ 指向 new 创建并返回的 线程对象，线程执行函数为 lambda表达式（创建了一个子线程，执行func_()）
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
            tid_ = CurrentThread::tid();    // 在新线程中，获取新线程的tid值
            sem_post(&sem);                 // 给信号量解锁, 信号量sem ++ ,并唤醒阻塞在信号量上的另一个进程或线程
            // 指向EventLoopThread的 threadFunc() 函数
            // 创建 EventLoop 对象，和此Thread对象对应
            func_(); }));   

    // 通过 信号量 来确保, 获取上面新建子线程 的tid值
    // 信号量 初值为0，则sem_wait()将阻塞，直到子线程的sem_post()返回
    sem_wait(&sem); // 给信号量加锁, 信号量sem --
}

// pthread_join()  —— 阻塞等待线程退出，获取线程退出状态
// 在EventLoopThread::~EventLoopThread()中调用
void Thread::join()
{
    joined_ = true;
    thread_->join();    // 当前Thread对象所在线程 阻塞回收，thread_指向的 新建的线程
}

void Thread::setDefaultName()
{
    // num : 当前创建的线程的编号
    int num = ++numCreated_; // numCreated_:静态数据成员，相当于 全局变量
    if (name_.empty()){
        char buf[32];
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}