#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/**
 * 从fd上读取数据  Poller工作在 LT模式（fd对应的 缓冲区内只要有数据，就会通知）
 * Buffer缓冲区是有大小的！ 但是从fd上读数据的时候，不知道tcp数据最终的大小
 * buffer_ 的vector申请的是 堆上的内存，
 */

// 读取fd中的数据  保存到 缓冲区中
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    char extrabuf[65536] = {0}; // 栈上的内存空间  64K

    struct iovec vec[2];
    // 指向 写缓冲区的首地址 writerIndex_
    const size_t writable = writableBytes(); // 这是Buffer底层缓冲区剩余的可写空间大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    // 从fd上读数据，会先保存vec[0]指向的缓冲区，如果空间不够，就会填充vec[1]指向的缓冲区
    // 如果extrabuf里有内容，就将extrabuf中的内容添加到 缓冲区里
    const ssize_t n = ::readv(fd, vec, iovcnt);     // 向多个buffers中 读写数据

    if (n < 0)
        *saveErrno = errno;
    else if (n <= writable) // Buffer的可写缓冲区已经够存储读出来的数据了
        writerIndex_ += n;
    else // extrabuf里面也写入了数据
    {  
        writerIndex_ = buffer_.size();
        // 对写缓冲区扩容，并将extrabuf里的n - writable个数据，copy到writerIndex_中，并将writerIndex_向后移动相应个位置
        append(extrabuf, n - writable); // writerIndex_开始写 n - writable大小的数据
    }
    return n;
}


ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    // 将peek()指向的buf 中的readableBytes()个字节，写入到文件fd中
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
        *saveErrno = errno;
    return n;
}