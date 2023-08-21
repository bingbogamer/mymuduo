#ifndef MUDUO_NET_ACCEPTOR_H
#define MUDUO_NET_ACCEPTOR_H

#include <functional>

#include "Channel.h"
#include "Socket.h"

class EventLoop;
class InetAddress;
///
/// Acceptor of incoming TCP connections.
///
class Acceptor : noncopyable
{
public:
    // 存储 可调用对象的 空function ： NewConnectionCallback
    typedef std::function<void(int sockfd, const InetAddress &)> NewConnectionCallback;

    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();
    
    void setNewConnectionCallback(const NewConnectionCallback &cb) { newConnectionCallback_ = cb; }

    bool listenning() const { return listenning_; }
    void listen();

private:
    void handleRead();

    EventLoop *loop_;           // 该loop_ 接收Tcpserver传入的mainloop
    Socket acceptSocket_;       // 用于监听连接的 listenfd
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_; // 存储可调用对象的 空function
    bool listenning_;
};

#endif
