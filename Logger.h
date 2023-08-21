#ifndef MUDUO_BASE_LOGGING_H
#define MUDUO_BASE_LOGGING_H
#include <string>

#include "noncopyable.h"


// 默认调试信息关闭
#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...)\
    do  \
    {   \
        Logger &logger = Logger::instance();    \
        logger.setLogLevel(DEBUG);\
        char buf[1024] = {0};\
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);\
        logger.log(buf);\
    }while(0)
#else   
    #define LOG_DEBUG(logmsgFormat, ...)    // 如果没有定义MUDEBUG，啥也不输出
#endif // MUDEBUG


// LOG_INFO("%s %d", arg1, arg2)
#define LOG_INFO(logmsgFormat, ...)\
    do  \
    {   \
        Logger &logger = Logger::instance();    \
        logger.setLogLevel(INFO);\
        char buf[1024] = {0};\
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);\
        logger.log(buf);\
    }while(0)


#define LOG_ERROR(logmsgFormat, ...)\
    do  \
    {   \
        Logger &logger = Logger::instance();    \
        logger.setLogLevel(ERROR);\
        char buf[1024] = {0};\
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);\
        logger.log(buf);\
    }while(0)

//  fatal: 致命的\灾难性的
#define LOG_FATAL(logmsgFormat, ...)\
    do  \
    {   \
        Logger &logger = Logger::instance();    \
        logger.setLogLevel(FATAL);\
        char buf[1024] = {0};\
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);\
        logger.log(buf);\
        exit(-1);\
    }while(0)



/* 定义日志级别  INFO ERROR FATAL DEBUG*/ 
enum LogLevel
{
    INFO,  // 普通信息
    ERROR, // 错误信息
    FATAL, // core信息
    DEBUG, // 调试信息
};

// 输出一个 日志类
class Logger : noncopyable
{
public:
    // 获取日志唯一 的实例对象
    static Logger &instance();          // 类静态成员函数：为类的全体对象服务，替代全局变量的使用
    // 设置日志级别
    void setLogLevel(int level);
    // 写日志
    void log(std::string msg);

private:
    int logLevel_; // 枚举类
    Logger(){};    // 构造函数
};

#endif // !