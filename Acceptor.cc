#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

static int createNonblockingOrDie()
{
    // 创建 非阻塞的 用于监听连接的 listenfd
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0)
        LOG_FATAL("%s:%s:%d listen socket createNonblockingOrDie error:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    return sockfd;
}

// Constructor, 在 Tcpserver 构造函数中 new Acceptor调用
// loop : 接收Tcpserver传入的mainloop
// listenAddr：Tcpserver传入的 listenAddr
// reuseport：Tcpserver传入的 listenAddr
Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop),   
      acceptSocket_(createNonblockingOrDie()),  // 创建 listenfd
      acceptChannel_(loop, acceptSocket_.fd()), // loop: Tcpserver传入的mainloop  这里创建的listenfd
      listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    // 绑定 listenfd 到 Tcpserver传入的 listenAddr（服务器主机地址结构）
    acceptSocket_.bindAddress(listenAddr);  // listenAddr：Tcpserver传入的 listenAddr

    // TcpServer::start()   Acceptor::listen() 有新用户的连接，要执行一个回调 （connfd -> channel -> subloop）
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

// Destructor
Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen(); // listen
    acceptChannel_.enableReading(); // acceptChannel_ => Poller
}

// listenfd 有事件发生，即有新用户连接了
void Acceptor::handleRead()
{
    InetAddress peerAddr;   // 保存传出的 连接的 <客户端> 的地址结构（IP+port）
    // 返回连接的 客户端的 connfd
    int connfd = acceptSocket_.accept(&peerAddr);  // peeraddr: 连接的 <客户端> 的地址结构（IP+port）
    if (connfd >= 0){
        // newConnectionCallback_ 在 TcpServer的构造函数里设置：绑定到TcpServer::newConnection函数
        if (newConnectionCallback_)
            // 传入 连接的客户端的 connfd、地址结构（IP+port）
            newConnectionCallback_(connfd, peerAddr); // 轮询找到subLoop，唤醒，分发当前的新客户端的Channel
        else
            ::close(connfd);
    }
    else{   // accept()出错,返回-1
        LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE)
            LOG_ERROR("%s:%s:%d sockfd reached limit! \n", __FILE__, __FUNCTION__, __LINE__);
    }
}
