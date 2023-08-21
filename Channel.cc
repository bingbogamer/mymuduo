#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; // 两个位变为1
const int Channel::kWriteEvent = EPOLLOUT;          // 0100

// 构造函数
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{
}

Channel::~Channel(){}

// 让弱指针 指向 shared_ptr
// 一个TcpConnection新连接创建的时候 TcpConnection => Channel 
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;   // std::weak_ptr<void> tie_;
    tied_ = true; // 标记已绑定
}


void Channel::update()
{
    loop_->updateChannel(this); 
}

// 在Channel所属的 EventLoop中，把当前的channel删除掉
// typedef std::vector<Channel*> ChannelList;
void Channel::remove()
{
    loop_->removeChannel(this);
}

// fd得到poller通知以后，处理事件 ——— 调用相应的回调方法
void Channel::handleEvent(Timestamp receiveTime)
{
    std::shared_ptr<void> guard;
    // 当前的Channel对象 是否绑定过
    if (tied_){
        guard = tie_.lock(); // 判断对象是否被释放，如果释放，则返回空的shared_ptr
        if (guard)           // 当对象未被释放
            handleEventWithGruad(receiveTime);
    }
    else 
        handleEventWithGruad(receiveTime);
}

// 根据poller返回的事件，由Channel 执行相应的回调函数
void Channel::handleEventWithGruad(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvents revents_:%d\n", revents_);

    if ((revents_ & (EPOLLIN | EPOLLPRI))){                 // 读事件 EPOLLPRI：外带数据  比较紧急的数据
        if (readCallBack_)  readCallBack_(receiveTime);
    }
    if ((revents_ & EPOLLOUT)){
        if (writeCallBack_) writeCallBack_();               // 写事件
    }
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)){    // 当 EPOLLIN 未发生，并且 Channel 对端关闭
        if (closeCallBack_) closeCallBack_();
    }
    if ((revents_ & EPOLLERR)){                             // 错误
        if (errorCallBack_) errorCallBack_();
    }
}