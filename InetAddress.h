#ifndef MUDUO_NET_INETADDRESS_H
#define MUDUO_NET_INETADDRESS_H

#include <string>
#include <arpa/inet.h>
#include <netinet/in.h> // <arpa/inet.h>头文件里包含了此头文件

class InetAddress
{
public:
    // 构造函数，(16位端口号 + IP地址)
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1"); // 防止隐式转换（函数参数类型 ——> 类类型）
    explicit InetAddress(const sockaddr_in &addr) : addr_(addr) {}

    // 获取 IP IP+port信息
    std::string toIp() const; // const成员函数（含this），不能通过此函数 改变调用 这个函数的对象的内容，此函数仅能读取，不能赋值
    std::string toIpPort() const;
    uint16_t toPort() const;

    // const: 指针指向的对象值不可变
    const sockaddr_in *getSockAddr() const { return &addr_; }
    void setSockAddr(const sockaddr_in& addr) { addr_ = addr; }
    
private:
    sockaddr_in addr_;
};

#endif // !MUDUO_NET_