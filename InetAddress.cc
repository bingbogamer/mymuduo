#include "InetAddress.h"
#include <strings.h>
#include <string.h>

InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);                  // 主机字节序 to 网络字节序
    addr_.sin_addr.s_addr = inet_addr(ip.c_str()); //  主机字节序 to 网络字节序
}


std::string InetAddress::toIp() const
{
    // 从addr_成员里的 网络字节序的IP地址 转换成 主机字节序的IP地址
    char buf[64] = {0};
    inet_ntop(AF_INET, &addr_.sin_addr.s_addr, buf, sizeof(buf));
    return buf;
}


std::string InetAddress::toIpPort() const
{
    char buf[64] = {0};
    inet_ntop(AF_INET, &addr_.sin_addr.s_addr, buf, sizeof(buf));

    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port); // 获取主机字节序的 网络端口号
    sprintf(buf + end, ":%u", port);
    return buf;
}

uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
}


// #include <iostream>
// int main(){
//     InetAddress addr(8080);
//     std::cout << addr.toIpPort() << std::endl;

//     return 0;
// }