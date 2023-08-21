#ifndef MUDUO_NET_CALLBACKS_H
#define MUDUO_NET_CALLBACKS_H

#include <memory>
#include <functional>

class Buffer;
class TcpConnection;
class Timestamp;

typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;    // shared_ptr指针：TcpConnection类

// 传入参数： TcpConnection类型的 shared_ptr
typedef std::function<void (const TcpConnectionPtr&)> ConnectionCallback;
typedef std::function<void (const TcpConnectionPtr&)> CloseCallback;
typedef std::function<void (const TcpConnectionPtr&)> WriteCompleteCallback;

// 对端接收慢，发送块，数据溢出水位丢失
typedef std::function<void (const TcpConnectionPtr&, size_t)> HighWaterMarkCallback; 
   
// the data has been read to (buf, len)
typedef std::function<void (const TcpConnectionPtr&, Buffer *, Timestamp)> MessageCallback;

#endif