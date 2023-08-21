#include "Poller.h"
#include <stdlib.h>
#include "EPollPoller.h"

// Poller：抽象基类
// Poller::newDefaultPoller 静态成员函数，不属于任何类对象，不含有this指针，只能访问类的静态成员
Poller* Poller::newDefaultPoller(EventLoop *loop){
    if (::getenv("MUDOU_USE_POLL")){
        return nullptr;     // 生成poll实例
    }
    else{
        return new EPollPoller(loop);     // 生成epoll实例
    }
}

// “指向派生类的基类指针” 访问虚函数，访问的是派生类中的实现