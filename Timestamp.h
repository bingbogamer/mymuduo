#ifndef MUDUO_BASE_TIMESTAMP_H
#define MUDUO_BASE_TIMESTAMP_H
#include <iostream>
#include <string>

// 时间戳
class Timestamp
{
public:
    Timestamp(); // 默认构造函数
    explicit Timestamp(int64_t microSecondsSinceEpoch); // 转换构造函数(禁止隐式转换[实参类型 transto 该类类型]  只能直接初始化)

    static Timestamp now();     // 静态成员函数  不属于任何对象
    std::string toString() const; // 常量成员函数：不能通过此成员函数改变调用对象的内容，此函数仅可读取，不能赋值

private:
    int64_t microSecondsSinceEpoch_;
};

#endif // MUDUO_BASE_TIMESTAMP_H