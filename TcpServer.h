#ifndef MUDUO_NET_TCPSERVER_H
#define MUDUO_NET_TCPSERVER_H
/**
 * 用户使用muduo编写服务器程序
 */
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

// 对外的服务器编程使用的类
class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    enum Option
    {
        kNoReusePort, // 不端口复用
        kReusePort,   // 端口复用
    };

    // kNoReusePort :默认不重用端口
    TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg, Option option = kNoReusePort);
    ~TcpServer();

    // ============================ 用户层实现Callback，调用下面接口传入  ===============================
    // 设置线程初始化 callback
    void setThreadInitcallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    // 设置连接 callback
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    // 设置Message callback
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    // 设置WriteComplete callback
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    // 设置底层subloop的个数
    void setThreadNum(int numThreads) { threadPool_->setThreadNum(numThreads); }

    // 开启服务器监听
    void start();

private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop *loop_; // baseLoop 用户定义的loop

    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_; // 运行在mainLoop，任务就是监听新连接事件

    // one loop per thread
    std::shared_ptr<EventLoopThreadPool> threadPool_;

    ThreadInitCallback threadInitCallback_;       // loop线程初始化的回调

    ConnectionCallback connectionCallback_;       // 有新连接时的回调
    MessageCallback messageCallback_;             // 已连接用户 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调

    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_; // 保存所有客户端连接的 TcpConnection对象的指针
};

#endif