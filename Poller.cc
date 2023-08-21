#include "Poller.h"
#include "Channel.h"

Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{
}


bool Poller::hasChannel(Channel* channel) const
{
    // auto it = channels_.find(channel->fd());
    // find():返回指向第一个关键字为key的迭代器。
    // 如果key不在容器中，则返回一个尾后迭代器
    ChannelMap::const_iterator it = channels_.find(channel->fd());  
    // 当存在此形参channel的sockfd，在容器中时，并判断fd对应的channel是否对应
    return it != channels_.end() && it->second == channel;
}