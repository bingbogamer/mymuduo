#ifndef MUDUO_BASE_NONCOPYABLE_H
#define MUDUO_BASE_NONCOPYABLE_H

/* noncopyable 被继承以后，派生类对象可以正常 构造+析构，但是不能进行拷贝构造、赋值操作*/
class noncopyable
{
public:
    /*派生类不能拷贝构造、赋值*/
    noncopyable(const noncopyable &) = delete;            // 拷贝构造函数
    noncopyable &operator=(const noncopyable &) = delete; // 重载拷贝赋值运算符函数

protected:
    /* 默认构造函数 析构函数：派生类可以访问保护权限*/
    noncopyable() = default;
    ~noncopyable() = default;
};

#endif // MUDUO_BASE_NONCOPYABLE_H
