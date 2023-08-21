#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// 防止一个线程创建多个EventLoop    thread_local
// 不加__thread 的话  就是全局变量，所有线程共享
// __thread变量每一个线程有一份独立实体，各个线程的值互不干扰
__thread EventLoop *t_loopInThisThread = nullptr; // 全局的EventLoop *指针变量

// 定义默认的Poller  IO复用接口的 超时时间
const int kPollTimeMs = 10000;


// 全局函数
// 创建wakeupfd，用来notify 唤醒subReactor处理新来的channel
int createEventFd()
{
    // eventfd - create a file descriptor for event notification
    // 专门用于事件通知的文件描述符
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);   // 非阻塞
    if (evtfd < 0)
        LOG_FATAL("eventfd error:%d \n", errno);
    return evtfd;
}


EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),              // 保存创建当前EventLoop对象的线程ID
      wakeupFd_(createEventFd()),                   // 生成一个eventfd，每个EventLoop对象，都会有自己的eventfd
      poller_(Poller::newDefaultPoller(this)),      // new创建一个EPollPoller， 直接传入当前EventLoop对象
      wakeupChannel_(new Channel(this, wakeupFd_)), // unique_ptr<Channel> wakeupChannel_
      currentActivaChannel_(nullptr)
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);

    // 如果当前线程已经绑定了某个EventLoop对象了，那么该线程就无法创建新的EventLoop对象了
    if (t_loopInThisThread)
        // 防止当前线程 重复创建了EventLoop
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    else
        t_loopInThisThread = this; // 第一次创建EventLoop对象，则赋值

    // 设置wakeupfd的事件类型 以及 发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个eventloop都将监听 wakeupchannel的 EPOLLIN 读事件
    wakeupChannel_->enableReading();
}


EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}


// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping \n", this);

    // 将此EventLoop对象，通过调用 EPollPoller::poll( , activeChannels_),  
    // EventLoop拿到了它的poller给他返回的所有发生事件的channel列表
    while (!quit_) // unique_ptr<Poller> poller_;
    {
        // ChannelList activeChannels_;  
        activeChannels_.clear();    // 清空vector
        // poll() -- epoll_wait()
        // 监听2类fd， 一种是client的fd   一种是wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_); 
        // “指向派生类的基类指针” 访问虚函数，访问的是派生类中的实现
        
        for (Channel *channel : activeChannels_){
            // currentActivaChannel_ = channel;
            channel->handleEvent(pollReturnTime_); // 处理事件 ——— 调用相应的回调方法
        }
        // 执行当前EventLoop事件循环 需要处理的回调操作
        /**
         * IO 线程  mainLoop  accept fd -> channel 在mainLoop中只做新用户的连接，要将这个channel分发给subLoop
         * mainLoop 实现注册一个回调cb (唤醒subLoop来执行注册的回调)
         */
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping \n", this);
    looping_ = false;
}


// 退出事件循环         1. loop在自己的线程中  调用quit
void EventLoop::quit()
{
    quit_ = true;
    // 如果是在其它线程中，调用的quit()     例如在一个subloop中，调用了mainLoop的quit
    if (!isInLoopThread())
        wakeup();
}


void EventLoop::runInLoop(Functor cb)
{
    // function<void()> Functor
    if (isInLoopThread())   cb();   // 在当前loop线程中，执行cb
    else  queueInLoop(cb);          // 在非当前loop线程中 执行cb，就需要唤醒loop所在线程，执行cb
}


// 将cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb) // typedef std::function<void()> Functor;
{
    {
        std::unique_lock<std::mutex> lock(mutex_);  
        // vector<Functor> pendingFunctors_;
        pendingFunctors_.emplace_back(cb);  
    }

    // 唤醒相应的，需要执行上面回调操作的 loop的线程
    // callingPendingFunctors_：当前loop在执行回调，但是loop又有了新的回调
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();   // 唤醒loop所在线程
    }
}

// wakeupFd_ 可读事件 回调函数
void EventLoop::handleRead()
{
    uint64_t one = 1; // 8字节
    // 从wakeupFd_中读取8个字节，写入到 one中
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8 \n", n);
    }
}

// 唤醒 loop所在的线程 subReactor
void EventLoop::wakeup()
{
    // 向wakeupfd_(eventfd)写值，如果写入的值和小于0xFFFFFFFFFFFFFFFE则写入成功
    // 如果大于0xFFFFFFFFFFFFFFFE
    // 如果设置了EFD_NONBLOCK标志位就直接返回-1
    // 如果没有设置EFD_NONBLOCK标志位就会一直阻塞到read操作执行。
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
}

void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}


void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}


void EventLoop::doPendingFunctors() // 执行回调
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;    // 当前loop在执行回调

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);       // pendingFunctors_变为空
    }

    for (const Functor &functor : functors)
    {
        functor();  // 执行当前loop需要执行的回调操作
    }
    callingPendingFunctors_ = false;
}