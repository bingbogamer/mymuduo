#ifndef MUDUO_NET_SOCKET_H
#define MUDUO_NET_SOCKET_H

#include "noncopyable.h"
class InetAddress;

class Socket : noncopyable
{
public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}

    // Socket(Socket&&) // move constructor in C++11
    ~Socket() { close(sockfd_); }

    int fd() const { return sockfd_; }

    // 如果地址正在使用，则中止
    void bindAddress(const InetAddress &localaddr);

    void listen();

    int accept(InetAddress *peeraddr);

    void shutdownWrite();

    void setTcpNoDelay(bool on); /// Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).
    void setReuseAddr(bool on);  // Enable/disable SO_REUSEADDR
    void setReusePort(bool on);  // Enable/disable SO_REUSEPORT
    void setKeepAlive(bool on);  // Enable/disable SO_KEEPALIVE

private:
    const int sockfd_;
};

#endif // MUDUO_NET_SOCKET_H