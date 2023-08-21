#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

#include <unistd.h>
#include <stdio.h>     // snprintf
#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>


// 将
void Socket::bindAddress(const InetAddress &localaddr)
{
    int ret = ::bind(sockfd_, (sockaddr *)localaddr.getSockAddr(), sizeof(sockaddr_in));
    if (ret != 0)
        LOG_FATAL("bind sockfd:%d fail \n", sockfd_);
}

void Socket::listen()
{
    int ret = ::listen(sockfd_, 1024); // 最多允许有1024个客户端处于连接待状态
    if (ret != 0)
        LOG_FATAL("listen sockfd:%d fail \n", sockfd_);
}

// peeraddr: 接收 连接的 <客户端> 的地址结构（IP+port）
// 返回连接的 客户端的 connfd
int Socket::accept(InetAddress *peeraddr)
{
    sockaddr_in client_addr;
    socklen_t clit_addr_len;
    bzero(&client_addr, sizeof client_addr);
    /**
     * client_addr: 传出参数—— 连接的 <客户端> 的地址结构（IP+port）
     * clit_addr_len: 传入传出参数
     * 传入：调用者提供的缓冲区 addr的长度以避免缓冲区溢出
     * 传出：客户端地址结构体 client_addr 的实际长度(可能 没有占满)
     */
    int connfd = ::accept(sockfd_, (sockaddr *)&client_addr, &clit_addr_len);
    if (connfd >= 0)
        peeraddr->setSockAddr(client_addr); // 将连接的 <客户端> 的地址结构 赋值给 peeraddr
    return connfd;
}

// 仅关闭写端
void Socket::shutdownWrite()
{
    if (::shutdown(sockfd_, SHUT_WR) < 0)
        LOG_ERROR("shutdownWrite error");
}


// Nagle算法就是为了尽可能发送大块数据，避免网络中充斥着许多小数据块。
void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    // TCP/IP协议中针对TCP默认开启了Nagle算法。
    // Nagle算法通过减少需要传输的数据包，来优化网络。在内核实现中，数据包的发送和接受会先做缓存，分别对应于写缓存和读缓存。
    // Nagle算法就是为了尽可能发送大块数据，避免网络中充斥着许多小数据块。
    // 启动TCP_NODELAY，禁用了Nagle算法，允许小包的发送。
    // 对于延时敏感型，同时数据传输量比较小的应用，开启TCP_NODELAY选项无疑是一个正确的选择
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof optval));
    // FIXME CHECK
}

void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof optval));
    // FIXME CHECK
}

void Socket::setReusePort(bool on)
{
#ifdef SO_REUSEPORT
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on)
        LOG_FATAL("SO_REUSEPORT failed.");
#else
    if (on)
        LOG_ERROR("SO_REUSEPORT is not supported.");
#endif
}

void Socket::setKeepAlive(bool on) 
{
    // on = True, optval = 1 ;  on = false, optval = 0
    int optval = on ? 1 : 0;    
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof optval));
    // FIXME CHECK
}

// 如果通信两端超过2个小时没有交换数据，那么开启keep-alive的一端会自动发一个keep-alive包给对端。
// 如果对端正常的回应ACK包，那么一切都没问题，再等个2小时后发包(如果这两个小时仍然没有数据交换)。
// 如果对端回应RST包，表明对端程序崩溃或重启，这边socket产生ECONNRESET错误，并且关闭。
// 如果对端一直没回应，这边会每75秒再发包给对端，总共发8次共11分钟15秒。
// 最后socket产生 ETIMEDOUT 错误，并且关闭。或者收到ICMP错误，表明主机不可到达，则会产生 EHOSTUNREACH 错误。

// 尽管TCP有SO_KEEPALIVE，但是它的间隔太长，并且只能判定对端的TCP连接是否正常，无法判断其他异常情况。
// 通常应用层都会自己设计心跳机制