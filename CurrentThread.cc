#include "CurrentThread.h"


namespace CurrentThread
{
    // g++ 编译器的 线程局部存储  类似于  C++ 11 的 thread_local 关键字
    // 每个线程都会有该变量的一个拷贝，互不干扰。该局部变量一直都在，直到线程退出为止。
    __thread int t_cachedTid;

    void cacheTid(){
        if (t_cachedTid == 0)
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
    }
} // namespace CurrentThread
