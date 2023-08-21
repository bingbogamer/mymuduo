#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <strings.h>
#include <functional>

// 判断传入的 mainloop是否为空
static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    return loop;
}

// TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg, Option option = kNoReusePort); 
// loop: 用户创建的 mainloop
// listenAddr: 用户设置的 listenAddr
// nameArg: 用户设置的 服务器名
TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg, Option option)
    : loop_(CheckLoopNotNull(loop)),
      ipPort_(listenAddr.toIpPort()),   // 主机 IP + 监听端口号
      name_(nameArg),       // EchoServer-01
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),// 创建listenfd 设置回调函数
      threadPool_(new EventLoopThreadPool(loop, name_)),// threadPool_ ：shared_ptr类型
      connectionCallback_(),
      messageCallback_(),
      nextConnId_(1),   // 已连接的客户端数量
      started_(0)
{
    // 当有先用户连接时，会执行TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
                                                  std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for (auto &item : connections_){
        // conn：局部的shared_ptr智能指针对象，出作用域，可以自动释放new出来的TcpConnection对象资源了
        TcpConnectionPtr conn(item.second);
        item.second.reset();    // 释放原指针

        // 销毁连接
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

// 开启服务器监听   loop.loop()
void TcpServer::start()
{
    // 防止一个TcpServer对象被start多次
    if (started_++ == 0) // 先判断started_ == 0，再started_++
    {
        threadPool_->start(threadInitCallback_); // 启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// 有一个新的客户端的连接，acceptor会执行这个回调操作，在Acceptor::handleRead()中会 调用 本函数
// sockfd : 传入连接客户端的 connfd
// peerAddr: 传入连接客户端的 peerAddr
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    char buf[64] = {0};
    // 格式化输出字符串，并将结果写入到指定的缓冲区，与 sprintf() 不同，snprintf() 会限制输出的字符数，避免缓冲区溢出。
    // int snprintf(char *str, size_t size, const char *format, ...)
    // 将字符串复制到 str 中，size 为要写入的字符的最大数目，超过 size 会被截断，最多写入 size-1 个字符
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    // name_：服务器对象名 EchoServer-01  ipPort_：主机 IP + 监听端口号
    std::string connName = name_ + buf;         // EchoServer-01 - 127.0.0.1 8000  #1

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
             name_.c_str(),     // 服务器对象名 EchoServer-01
             connName.c_str(),  // EchoServer-01 - 127.0.0.1 8000  #1
             peerAddr.toIpPort().c_str()    // 客户端 IP + 端口号
    );

    // 通过sockfd 获取其绑定的本机的ip地址和端口信息
    // sockets::getLocalAddr
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    // sockfd : 传入连接客户端的 connfd
    // local: 由内核赋予该连接的本地IP地址和本地端口号
    if (::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0)
        LOG_ERROR("sockets::getLocalAddr");
    InetAddress localAddr(local);

    EventLoop *ioLoop = threadPool_->getNextLoop(); // 轮询算法，选择一个subLoop，来管理channel
    // 根据连接成功的sockfd，创建TcpConnection连接对象
    // shared_ptr<TcpConnection>  conn -> 指向 new出来的 TcpConnection（和connfd绑定）
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));    // new Socket + Channel

    // 用户=>TcpServer=>TcpConnection=>Channel=>Poller=>notify channel调用回调
    // 将 TcpServer的 Callback成员：connectionCallback_、messageCallback_、writeCompleteCallback_ 赋值给 新建的 TcpConnection对象
    // 这3个成员 是用户 传入的
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    // 设置了如何关闭连接的回调   conn->shutDown()
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
    connections_[connName] = conn;

    // 直接调用TcpConnection::connectEstablished
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n", name_.c_str(), conn->name().c_str());

    size_t n = connections_.erase(conn->name());
    (void)n;
    assert(n == 1);

    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}