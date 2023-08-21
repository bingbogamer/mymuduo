#ifndef MUDUO_NET_CHANNEL_H
#define MUDUO_NET_CHANNEL_H
#include <functional>
#include <memory>

#include "noncopyable.h"
#include "Timestamp.h"

class EventLoop;

/* channel 为一个可选择的I/O通道通道，这个类不拥有文件描述符。文件描述符可以是socket、eventfd、timerfd或signalfd
    封装了 sockfd 和 其他感兴趣的 event ,如 EPOLLIN、EPOLLOUT事件
    还绑定了 poller返回的具体事件

    1个channel对象负责  一个fd
*/
class Channel : noncopyable
{
public:
    typedef std::function<void()> EventCallback;              // 事件回调函数
    typedef std::function<void(Timestamp)> ReadEventCallback; // 只读事件回调

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd得到poller通知以后，处理事件 ——— 调用相应的回调方法
    void handleEvent(Timestamp receiveTime);

    void setReadCallback(ReadEventCallback cb) { readCallBack_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallBack_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallBack_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallBack_ = std::move(cb); }

    // 防止当 channel 被手动remove掉，channel还在执行上面4个回调操作
    // 将这个通道绑定到由shared_ptr管理的所有者对象，防止所有者对象在handleEvent中被销毁
    void tie(const std::shared_ptr<void> &);

    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; }

    // 设置fd相应的 事件状态
    void enableReading(){ events_ |= kReadEvent; update();}     // Channel 添加 可读事件  并监听
    void disableReading(){ events_ &= ~kReadEvent; update();}   // Channel 删除 可读事件  不监听
    void enableWriting(){ events_ |= kWriteEvent; update();}    // Channel 添加 可写事件  并监听
    void disableWriting(){ events_ &= ~kWriteEvent; update();}  // Channel 删除 可写事件  不监听
    void disableAll(){ events_ = kNoneEvent; update();}         // Channel 对所有事件     都不监听

    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }  // 判断是否有事件发生
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    // for Poller
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // one loop per thread
    EventLoop *ownerloop() { return loop_; }
    void remove(); // 用于删除 channel

private:
    void update();
    void handleEventWithGruad(Timestamp receiveTime); 

    static const int kNoneEvent;  // 没有事件
    static const int kReadEvent;  // 读事件
    static const int kWriteEvent; // 写事件

    EventLoop *loop_; // 事件循环
    const int fd_;    // fd,   Poller监听的对象
    int events_;      // 注册fd 感兴趣的事件
    int revents_;     
    int index_;    

    std::weak_ptr<void> tie_; 
    bool tied_;           


    ReadEventCallback readCallBack_; 
    EventCallback writeCallBack_;    
    EventCallback closeCallBack_;   
    EventCallback errorCallBack_;    
};

#endif //