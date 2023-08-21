#pragma once

#include <assert.h>
#include <vector>
#include <string>
#include <algorithm>

/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |      8字节        |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size

// 网络库底层的缓冲器类型定义
class Buffer
{
public:
    // 静态数据成员（通常情况下 必须类外定义、初始化）
    // 可以为静态数据成员提供 const整数类型的 类内初始值，但是要求必须是字面值常量类型的 constexpr
    static const size_t kCheapPrepend = 8;      // prependable bytes
    static const size_t kInitialSize = 1024;    // readable bytes  |  writable bytes

    // 不能隐式转换
    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)   // readerIndex_ = writerIndex_ = kCheapPrepend
        , writerIndex_(kCheapPrepend)   // 读写缓冲区索引 初始值相等
    {
        assert(readableBytes() == 0);
        assert(writableBytes() == initialSize);
        assert(prependableBytes() == kCheapPrepend);
    }
    
    size_t readableBytes() const { return writerIndex_ - readerIndex_; }    // 读缓冲区长度
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }  // 写缓冲区长度
    size_t prependableBytes() const { return readerIndex_; }                 

    // 读缓冲区首地址
    const char *peek() const 
    { 
        return begin() + readerIndex_; 
    }

    // onMessage string <- Buffer
    // 注意：传入的 len的大小 必须 <= 读缓冲区当前的长度
    // 判断 读缓冲区长度 是小于 len 还是 == len 
    // 如果 读缓冲区长度 > len，则将 readerIndex_往后移动 len 个 byte
    // 如果 读缓冲区长度 == len，则将 readerIndex_ 、writerIndex_都往前移动到 kCheapPrepend的初始位置
    void retrieve(size_t len)
    {
        assert(len <= readableBytes());
        if (len < readableBytes())
            // 应用只读取了读缓冲区数据的一部分，就是len，还剩下readerIndex_ += len -> writerIndex_
            readerIndex_ += len;    // readerIndex_往后移动 len 个 byte
        else 
            // len == readableBytes() , 说明写入的len个数据将占满 读缓冲区
            // 然后readerIndex_读缓冲区 从头开始，大小为0 ，写缓冲区writerIndex_ 也从头开始
            retrieveAll();
    }

    // 将 readerIndex_  writerIndex_ 和 kCheapPrepend重合
    void retrieveAll() { readerIndex_ = writerIndex_ = kCheapPrepend; }

    // 将当前的读缓冲区中的 char数据 全部转换为1个 string对象
    // 并调用retrieveAll() 使 readerIndex_ = writerIndex_ = kCheapPrepend; 
    std::string retrieveAllAsString(){ return retrieveAsString(readableBytes());} // 应用可读取数据的长度
    
    // 将 读缓冲区首地址readerIndex_ 开始的 len个char字符 构造并返回 string对象
    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        // 前提 len <= readableBytes()
        retrieve(len); // 上面一句把缓冲区中可读的数据，已经读取出来，这里肯定要对缓冲区进行复位操作
        return result;
    }

    // buffer_.size() - writerIndex_    len
    void ensureWriteableBytes(size_t len)
    {
        if (writableBytes() < len)
            makeSpace(len);         // 扩容函数
        assert(writableBytes() >= len);
    }

    // 把[data, data+len]内存上的数据，添加到 writable缓冲区当中
    void append(const char *data, size_t len)
    {
        ensureWriteableBytes(len);  // 确保有足够的可写空间，如果没有则扩容
        std::copy(data, data + len, beginWrite());  // 将len个数据 写到 writerIndex_ 开始的写缓冲区中
        writerIndex_ += len;        // writerIndex_往后移动 len 个 byte
    }

    char *beginWrite() { return begin() + writerIndex_; }
    const char *beginWrite() const { return begin() + writerIndex_; }   // 返回 写缓冲区的首元素地址

    // 从fd上读取数据 到buffer_缓冲区
    ssize_t readFd(int fd, int *saveErrno);
    // 向fd发送数据   先发送到buffer_缓冲区
    ssize_t writeFd(int fd, int *saveErrno);

private:
    // buffer_.begin() 指向vector首元素的迭代器   &*it 首元素的地址
    // （vector扩容的话 数据要全部移动，所以要取首地址）
    char *begin(){ return &*buffer_.begin(); } // vector底层数组首元素的地址，也就是数组的起始地址
    const char *begin() const{return &*buffer_.begin();}

    // 
    void makeSpace(size_t len) {
        // 如果将 当前的写缓冲区size + 之前读取过数据的 读缓冲区size < len
        // 说明即时将readable中 未读的数据，拷贝到前面去，也不能够写入全部的len个数据，要进行扩容
        if (writableBytes() + prependableBytes() - kCheapPrepend < len){
            buffer_.resize(writerIndex_ + len);
        }
        // 如果移动后，能够将所有的len个数据 全部放入 writerIndex_区
        else{
            size_t readalbe = readableBytes();
            // | prependable bytes |  readable bytes  |  writable bytes  |
            // 将readable中 未读的数据，拷贝到前面去（移动数据）
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readalbe;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};