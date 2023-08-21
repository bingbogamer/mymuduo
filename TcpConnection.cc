#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <functional>
#include <errno.h>
#include <sys/types.h>         
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <string>

static EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
        LOG_FATAL("%s:%s:%d TcpConnection ioLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    return loop;
}

//在 TcpServer::newConnection(connfd, peerAddr) 中调用
// TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));  // new Socket + Channel
TcpConnection::TcpConnection(EventLoop *loop,           // threadPool_->getNextLoop() 返回的 subloop
                            const std::string &nameArg, // connName
                            int sockfd,                 // 传入连接客户端的 connfd
                            const InetAddress& localAddr,// 与客户端连接的 本地主机地址结构
                            const InetAddress& peerAddr)//  传入连接客户端的 peerAddr地址结构
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnecting)   // 初始状态：正在连接
    , reading_(true)
    , socket_(new Socket(sockfd))           // unique_ptr socket_ <- connfd
    , channel_(new Channel(loop, sockfd))   // unique_ptr channel_ <- connfd
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64*1024*1024) // 水位线设置：64M
{
    // 下面给channel设置相应的回调函数，poller给channel通知感兴趣的事件发生了，channel会回调相应的操作函数
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);    // 设置TCP心跳
}


TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n", name_.c_str(), channel_->fd(), (int)state_);
}

// send: 发动给客户端 处理完成的数据
void TcpConnection::send(const std::string &buf)
{
    if (state_ == kConnected){
        // 当前的loop_是不是在自己对应的 线程里
        if (loop_->isInLoopThread())
            sendInLoop(buf.c_str(), buf.size());
        else
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
    }
}

/**
 * 发送数据  应用写的快， 而内核发送数据慢， 需要把待发送数据写入缓冲区， 而且设置了水位回调
 */ 
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;         // 已发送的数据
    size_t remaining = len;     // 没发送的数据
    bool faultError = false;

    // 之前调用过该connection的shutdown，不能再进行发送了
    if (state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }
    // if no thing in output queue, try writing directly
    // 表示channel_第一次开始写数据，而且缓冲区没有待发送数据
    // bool isWriting() const { return events_ & kWriteEvent; }
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        // 将data里的len个数据，写入channel_的fd设备文件
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0){
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_){
                // 既然在这里数据全部发送完成，就不用再给channel设置epollout事件了
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwrote < 0
        {
            nwrote = 0;
            // -1： errno = EAGIN 或 EWOULDBLOCK, 不是read失败，
            // 而是read在以非阻塞方式读一个设备文件（网络文件），并且文件无数据
            if (errno != EWOULDBLOCK){
                LOG_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET) // SIGPIPE  RESET
                {
                    faultError = true;
                }
            }
        }
    }

    // 说明当前这一次write，并没有把数据全部发送出去，剩余的数据需要保存到缓冲区当中，然后给channel
    // 注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知相应的sock-channel，调用writeCallback_回调方法
    // 也就是调用TcpConnection::handleWrite方法，把发送缓冲区中的数据全部发送完成
    assert(remaining <= len);
    if (!faultError && remaining > 0) 
    {
        // 目前发送缓冲区剩余的待发送数据的长度
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen+remaining));
        }
        outputBuffer_.append((char*)data + nwrote, remaining);
        if (!channel_->isWriting()){
            channel_->enableWriting(); // 这里一定要注册channel的写事件，否则poller不会给channel通知epollout
        }
    }
}

// 关闭连接
void TcpConnection::shutdown()
{
    if (state_ == kConnected){
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}


void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting())     // 说明outputBuffer中的数据已经全部发送完成
        // 关闭写端，触发Socket对应的Channel的EPOLLHUP，调用closeCallback(),继而调用TcpConnection::handleClose()
        socket_->shutdownWrite();   
}

// 连接建立
void TcpConnection::connectEstablished()
{
    assert(state_ == kConnecting);
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();          // 向poller注册channel的epollin事件

    // 新连接建立，执行回调
    connectionCallback_(shared_from_this());
}

// 连接销毁
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll(); // 把channel的所有感兴趣的事件，从poller中delete掉
        connectionCallback_(shared_from_this());
    }
    channel_->remove(); // 把channel从poller中删除掉
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    // 读取fd中的数据  保存到 inputBuffer_缓冲区中
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno); 
    if (n > 0){
        // 已建立连接的用户，有可读事件发生了，调用 用户传入的回调操作 onMessage
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)    // 断开了
        handleClose();
    else{
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if (channel_->isWriting()){
        int savedErrno = 0;
        // 将peek()指向的 outputBuffer_ 中readableBytes() 所有的字节，写入到文件fd中
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if (n > 0){
            outputBuffer_.retrieve(n);
            // 数据写完
            if (outputBuffer_.readableBytes() == 0){
                channel_->disableWriting();
                if (writeCompleteCallback_)
                    // 唤醒loop_对应的thread线程，执行回调
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                // TCP连接正在关闭（客户端断开，服务器shutdown）
                if (state_ == kDisconnecting)   
                    shutdownInLoop();
            }
        }
        else
            LOG_ERROR("TcpConnection::handleWrite");
    }
    else
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());
}

// poller => channel::closeCallback => TcpConnection::handleClose
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->fd(), (int)state_);
    // 只有在 已连接 或 正在断开连接 状态下才能close
    assert(state_ == kConnected || state_ == kDisconnecting);
    setState(kDisconnected);
    channel_->disableAll();         // Channel事件从Poller中删除， 对所有事件 都不监听

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);   // 执行有新连接时的回调
    closeCallback_(connPtr);        // 执行关闭连接的回调  执行的是TcpServer::removeConnection回调方法
}


// 只是单纯的获取了错误信息，并打印。
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    // 当一个socket发生错误的时候，将使用一个名为so_error的变量记录对应的错误代码，这又叫做pending error，
    // so_error为0时表示没有错误发生
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
        err = errno;
    else
        err = optval;
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}