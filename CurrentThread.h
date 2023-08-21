#ifndef MUDUO_BASE_CURRENTTHREAD_H
#define MUDUO_BASE_CURRENTTHREAD_H

#include <unistd.h>
#include <sys/syscall.h>

// 定义一个命名空间
namespace CurrentThread
{
    extern __thread int t_cachedTid; // 声明一个变量，而非定义

    void cacheTid();
    // 获取当前线程的  线程ID
    inline int tid(){
        if (__builtin_expect(t_cachedTid == 0, 0))  cacheTid();
        return t_cachedTid;
    }
}
#endif //