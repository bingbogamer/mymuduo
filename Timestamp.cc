#include "Timestamp.h"
#include <time.h>

Timestamp::Timestamp() : microSecondsSinceEpoch_(0) {}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}


Timestamp Timestamp::now()
{
    time_t ti = time(NULL);
    return Timestamp(ti);
}


std::string Timestamp::toString() const
{
    char buf[128] = {0};
    tm *tm_time = localtime(&microSecondsSinceEpoch_);
    // 格式化输出字符串，并将结果写入到buf缓冲区，与 sprintf() 不同，snprintf() 会限制输出的字符数，避免buf溢出
    // 如果格式化后的字符串长度超过了 size-1，则 snprintf() 只会写入 size-1 个字符，并在字符串的末尾添加一个空字符（\0）以表示字符串的结束
    snprintf(buf, 128, "%4d/%02d/%02d  %02d:%02d:%02d", // 输出  年月日  时分秒
             tm_time->tm_year + 1900,
             tm_time->tm_mon + 1,
             tm_time->tm_mday,
             tm_time->tm_hour,
             tm_time->tm_min,
             tm_time->tm_sec);
    return buf;
}

// #include <iostream>
// int main()
// {
//     std::cout << Timestamp::now().toString() << std::endl;
//     return 0;
// }