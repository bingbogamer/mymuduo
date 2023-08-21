#include "Logger.h"
#include <iostream>

// 获取日志唯一的 实例对象
Logger &Logger::instance()
{
    static Logger logger; // 静态局部变量
    return logger;
}


// 设置日志级别
void Logger::setLogLevel(int level)
{
    logLevel_ = level;
}

// 写日志 [级别信息] time : msg
void Logger::log(std::string msg)
{
    switch (logLevel_)
    {
    case INFO:
        std::cout << "[INFO]";
        break;
    case ERROR:
        std::cout << "[ERROR]";
        break;
    case FATAL:
        std::cout << "[FATAL]";
        break;
    case DEBUG:
        std::cout << "[DEBUG]";
        break;
    default:
        break;
    }

    // 打印时间 和 msg 
    std::cout << "print time" << " : " << msg << std::endl;
}
