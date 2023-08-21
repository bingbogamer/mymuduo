#  mymuduo核心类梳理

## C++语法部分

<img src="C:\Users\BingBo\AppData\Roaming\Typora\typora-user-images\image-20230620194749665.png" alt="image-20230620194749665" style="zoom:67%;" />

<img src="C:\Users\BingBo\AppData\Roaming\Typora\typora-user-images\image-20230620195552181.png" alt="image-20230620195552181" style="zoom:50%;" />

noncopyable类   删除了  拷贝构造函数 、重载赋值运算符函数，那么派生类对象的拷贝构造和赋值，是要先调用基类的拷贝构造和赋值，然后再是派生类特有的拷贝构造和赋值，但是base类的已经被delete了。所以这样写作用就是：让继承的**派生类不能拷贝构造、赋值**了。

默认构造函数、析构函数是protected的，所以  派生类可以访问，外部无法访问。因此，派生类对象的创建，会调用基类的构造函数，所以派生类对象的创建、析构不会受影响。 



## 信号量

利用信号量sem 来阻塞 主线程，确保 新线程中已经将 返回值赋给 tid_ 。

![image-20230811222717110](手写muduo项目.assets/image-20230811222717110.png)



## 互斥锁mutex

<img src="手写muduo项目.assets/image-20230812115643219.png" alt="image-20230812115643219" style="zoom:50%;" />

unique_lock< std::mutex > 自动上锁 ，除了作用域，自动解锁

<img src="手写muduo项目.assets/image-20230811232049700.png" alt="image-20230811232049700" style="zoom:67%;" />



<img src="手写muduo项目.assets/image-20230812151851250.png" alt="image-20230812151851250" style="zoom:50%;" />



## 条件变量

<img src="手写muduo项目.assets/image-20230812152736673.png" alt="image-20230812152736673" style="zoom: 50%;" />

<img src="手写muduo项目.assets/image-20230812152758637.png" alt="image-20230812152758637" style="zoom:50%;" />





## 原子变量（atomic）

```
C++中 原子变量（atomic）是一种多线程编程中常用的同步机制，它能够确保对共享变量的操作在执行时不会被其他线程的操作干扰，从而避免竞态条件（race condition）和死锁（deadlock）等问题。

原子变量可以看作是一种特殊的类型，它具有类似于普通变量的操作，但是这些操作都是原子级别的，即要么全部完成，要么全部未完成。C++标准库提供了丰富的原子类型，包括整型、指针、布尔值等，使用方法也非常简单，只需要通过std::atomic<T>定义一个原子变量即可，其中T表示变量的类型。

在普通的变量中，并发的访问它可能会导致数据竞争，竞争的后果会导致操作过程不会按照正确的顺序进行操作。
```

线程类的 Thread 的静态数据成员 numCreated_，不属于任何类对象，是Thread 类的单独数据



<img src="手写muduo项目.assets/image-20230811222955492.png" alt="image-20230811222955492" style="zoom: 67%;" />

防止在其他 EventLoop所在的线程中，同时对 共享变量numCreated_进行操作，导致返回的值不正确

<img src="手写muduo项目.assets/image-20230811223406291.png" alt="image-20230811223406291" style="zoom:67%;" />



# =====================================

# Channel（->  EventLoop ）

上层类：

**EventLoop**

作为EventLoop的数据成员

1. unique_ptr<Channel>   **wakeupChannel_**;  
2. vector < Channel * > ChannelList    activeChannels_
   3. Channel *    currentActivaChannel_;  

在 EventLoop 的构造函数中，new了一个Channel 对象，赋值给 **wakeupChannel_**

<img src="手写muduo项目.assets/image-20230812164409525.png" alt="image-20230812164409525" style="zoom: 50%;" />



主要成员：

fd_	当前Channel对应的

<img src="手写muduo项目.assets/image-20230723222240175.png" alt="image-20230723222240175" style="zoom: 67%;" />

#### 构造

初始化该Channel对象 对应的 loop 和  **监听的 fd**

<img src="手写muduo项目.assets/image-20230723222109290.png" alt="image-20230723222109290" style="zoom:67%;" />

#### set  Read/Write/Close/Error  Callback()

设置该 Channel 对应的 读写事件的 Callback

<img src="手写muduo项目.assets/image-20230723223327384.png" alt="image-20230723223327384" style="zoom: 67%;" />



在 TcpConnection构造函数 中调用

Channel::readCallBack_	->   TcpConnection::handleRead

Channel::writeCallBack_	->   TcpConnection::handleWrite

Channel::closeCallBack_	->   TcpConnection::handleClose

Channel::errorCallBack_	->   TcpConnection::handleError

<img src="手写muduo项目.assets/image-20230801230428225.png" alt="image-20230801230428225" style="zoom: 50%;" />

#### handleEventWithGruad（）

执行相应的回调函数

<img src="手写muduo项目.assets/image-20230723225158980.png" alt="image-20230723225158980" style="zoom: 67%;" />



提供给其他类调用的接口

<img src="手写muduo项目.assets/image-20230723223455824.png" alt="image-20230723223455824" style="zoom:67%;" />



传入一个共享指针， 让弱指针指向该对象，并标记为已经绑定 tied_

<img src="手写muduo项目.assets/image-20230723222608437.png" alt="image-20230723222608437" style="zoom: 67%;" />

当channel表示的 fd的 events事件改变后，update() 在poller里更改fd相应的事件 epoll_ctl

 \* 但是Channel和Poller之间是独立的，都属于EventLoop

 \* Channel的update() 和 remove() , 通过loop_调用EventLoop的updateChannel和removeChannel,再调用Poller的updateChannel和removeChannel

```C++
void Channel::update(){
    // 通过 channel 所属的 EventLoop, 调用 poller 的相应方法，注册fd的 events事件
    loop_->updateChannel(this); // 将此Channel对象 传给 指针loop_指向的EventLoop对象
}

// 在Channel所属的 EventLoop中，把当前的channel删除掉
// typedef std::vector<Channel*> ChannelList;
void Channel::remove(){
    loop_->removeChannel(this);
}
```

# Poller  （抽象基类 ->  EPollPoller）

重要成员： 

定义一个 Channel 的哈希表，派生类 EPollPoller 会继承 这个成员，用来存储当前这个 Poller中 的所有 Channel

<img src="手写muduo项目.assets/image-20230723225749103.png" alt="image-20230723225749103" style="zoom:67%;" />

#### 构造

传入所属的  EventLoop 对象指针，表示此 Poller所属的 EventLoop 

<img src="手写muduo项目.assets/image-20230724214002621.png" alt="image-20230724214002621" style="zoom:67%;" />

#### hasChannel( Channel *channel )   

// 判断 参数channel是否在当前Poller中 

<img src="手写muduo项目.assets/image-20230724213752514.png" alt="image-20230724213752514" style="zoom:67%;" />

#### newDefaultPoller( EventLoop *loop ）

<img src="手写muduo项目.assets/image-20230724215739716.png" alt="image-20230724215739716" style="zoom:67%;" />





# EPollPoller（->  EventLoop ）

上层类：

**EventLoop**

作为EventLoop的数据成员  unique_ptr<Poller> poller_;

在 EventLoop 的构造函数中，new了一个Channel 对象，赋值给 wakeupChannel_

<img src="手写muduo项目.assets/image-20230812164409525.png" alt="image-20230812164409525" style="zoom: 50%;" />



主要成员：

events_ ：  存储 epoll_event 事件的数组 —— epoll_wait () 的传出参数 数组

epollfd_   :   efd 红黑树根

![image-20230724220634728](手写muduo项目.assets/image-20230724220634728.png)



#### 构造 — epoll_create1

传入所属的  EventLoop 对象指针，表示此 EPollPoller 所属的 EventLoop 

创建 efd 红黑树根

![image-20230724220129671](手写muduo项目.assets/image-20230724220129671.png)

#### 析构

<img src="手写muduo项目.assets/image-20230724221139173.png" alt="image-20230724221139173" style="zoom: 67%;" />

#### poll ()  —  epoll_wait  —  扩容

发生事件的 fd ，都存储在 数组中

如果满足监听事件的 返回fd数量，达到了数组的size，则对数组 进行扩容 2 倍

![](手写muduo项目.assets/image-20230724223751966.png)

![](手写muduo项目.assets/image-20230724224848654.png)



# =====================================

# EventLoop（-> EventLoopThread）

上层类：

**EventLoopThread**

作为 EventLoopThread 的数据成员 EventLoop *loop_

在  EventLoopThread::threadFunc() 中  创建了 EventLoop loop，并在新线程中将 

<img src="手写muduo项目.assets/image-20230812182724253.png" alt="image-20230812182724253" style="zoom:50%;" />



主要成员

wakeupFd_ ：每一个Loop 对应一个 wakeupFd_ 

![image-20230724230316320](手写muduo项目.assets/image-20230724230316320.png)



#### 构造  —  wakeupFd_  \ poller_

wakeupFd_  ： 创建1个 非阻塞的 fd，用来唤醒 subReactor 处理新来的channel

wakeupChannel_  ： 创建一个Channel， 该Channel 对应 wakeupFd_  ，并设置 

poller_  :  创建了一个 Poller对象

![image-20230724225739366](手写muduo项目.assets/image-20230724225739366.png)



#### loop()

调用 poll ，监听

![image-20230724232230033](手写muduo项目.assets/image-20230724232230033.png)



#### runInLoop（）

如果调用当前方法的线程ID  是 创建EventLoop对象的线程，执行传入的回调函数

<img src="手写muduo项目.assets/image-20230730222711107.png" alt="image-20230730222711107" style="zoom:67%;" />

#### queueInLoop()



<img src="手写muduo项目.assets/image-20230812224337960.png" alt="image-20230812224337960" style="zoom:67%;" />

#### handleRead()





#### wakeup()

将one中的值 写入到 wakeupFd_ 中， 唤醒 当前loop对象 所在的线程

<img src="手写muduo项目.assets/image-20230812223922903.png" alt="image-20230812223922903" style="zoom: 67%;" />





# Thread（->  EventLoopThread ）

one loop per thread  一个EventLoop 对应一个 线程

 上层类：

**EventLoopThread**

作为 EventLoopThread 的数据成员  Thread  thread_，并在EventLoopThread 的构造函数初始化类表中初始化，

<img src="手写muduo项目.assets/image-20230812153013405.png" alt="image-20230812153013405" style="zoom:50%;" />





成员：

func_ ： function<void()>  func_ 函数指针，在EventLoopThread 的构造函数中，传入赋值

<img src="手写muduo项目.assets/image-20230812151011007.png" alt="image-20230812151011007" style="zoom:50%;" />







#### 构造

传入 线程函数func ，赋值给 func_ 成员

<img src="手写muduo项目.assets/image-20230811204646560.png" style="zoom:67%;" />



#### start() 	创建新线程per thread

创建新的线程，并执行 func_()

![image-20230718180537974](手写muduo项目.assets/image-20230718180537974.png)

#### join()

调用处： EventLoopThread::~EventLoopThread()

当前Thread对象所在线程，阻塞回收 thread_指向的 新建的线程

<img src="手写muduo项目.assets/image-20230811231207735.png" alt="image-20230811231207735" style="zoom:67%;" />







# EventLoopThread ( ->  EventLoopThreadPool )

上层类：

**EventLoopThreadPool**

在 EventLoopThreadPool::start() 中  ——>   new 了一个 EventLoopThread 对象

<img src="手写muduo项目.assets/image-20230802211151327.png" alt="image-20230802211151327" style="zoom: 50%;" />



成员：

有一个Thread成员  thread_  

<img src="手写muduo项目.assets/image-20230718203155632.png" alt="image-20230718203155632" style="zoom:67%;" />

#### 构造

重点： 将成员函数 threadFunc() 赋值给 Thread thread_成员的 func_成员，这样 在startLoop() 中 调用 thread_.start();  

启动底层的新线程,  来执行threadFunc() 函数

![image-20230718203438446](手写muduo项目.assets/image-20230718203438446.png)



#### threadFunc()   	 创建oneloop，运行在新线程中

在新线程， 栈空间创立一个EventLoop对象，赋值给 loop_成员，并开始事件循环

<img src="手写muduo项目.assets/image-20230718203534927.png" alt="image-20230718203534927" style="zoom:67%;" />





#### startLoop()

在执行 startLoop() 的线程中，在thread_.start() 后，

先创建一个 EventLoop *loop = NULL;  空指针

当前线程  试图获取 上锁，尝试 

获取新建的子线程 中

<img src="手写muduo项目.assets/image-20230718210823963.png" alt="image-20230718210823963" style="zoom:67%;" />





新线程中：

第一步：

thread_.start()  ——  new std::thread 创建 底层的新线程，

这个新线程，执行  func_()，即下面的 EventLoopThread:: threadFunc() 函数，

<img src="手写muduo项目.assets/image-20230812185625166.png" alt="image-20230812185625166" style="zoom:50%;" />

第二步：

先拿到 

执行 threadFunc() ：执行loop.loop()，开启事件循环。 新线程会长时间在这里执行



# =====================================

# EventLoopThreadPool（ ->  TcpServer）

上层类：

**TcpServer**

在TcpServer构造函数  初始化列表中  new了一个 的 EventLoopThreadPool对象

传入参数：  // loop: 用户创建的 mainloop		// listenAddr: 用户设置的 listenAddr

![image-20230802205356665](手写muduo项目.assets/image-20230802205356665.png)



重要成员

loops_  :  Eventloop 指针 数组，存储所有

![image-20230718205837497](手写muduo项目.assets/image-20230718205837497.png)

#### 构造

传入所属的  EventLoop 对象指针，表示此 EventLoopThreadPool 所属的 baseloop 

baseloop 由用户 在调用网络库 时 创建传入

<img src="手写muduo项目.assets/image-20230718205927399.png" alt="image-20230718205927399" style="zoom:67%;" />

线程绑定的 loop 是栈上的对象，不用delete 释放

<img src="手写muduo项目.assets/image-20230718210257483.png" alt="image-20230718210257483" style="zoom:67%;" />

#### start() 

此start()  由 TcpServer::start() 中，执行 调用

<img src="手写muduo项目.assets/image-20230802211048830.png" alt="image-20230802211048830" style="zoom:50%;" />

new 一个 EventLoopThread 对象，

![image-20230802211151327](手写muduo项目.assets/image-20230802211151327.png)





# Socket  （-> Acceptor）









# Acceptor（-> TcpServer）

数据成员

<img src="手写muduo项目.assets/image-20230801183405727.png" alt="image-20230801183405727" style="zoom:67%;" />

#### 构造函数

Channel  **acceptChannel_**   :  由 mainloop构造，属于 mainloop

创建 listenfd

acceptSocket_.bindAddress(listenAddr) : 绑定 listenfd 到 Tcpserver传入的 listenAddr（服务器主机地址结构）

acceptChannel_.setReadCallback( bind(&Acceptor::handleRead, this)) :   

​	设置 acceptChannel_ 的 readCallBack_ -> Acceptor::handleRead（）

![image-20230801193558085](手写muduo项目.assets/image-20230801193558085.png)

#### setNewConnectionCallback

![image-20230801190621320](手写muduo项目.assets/image-20230801190621320.png)

#### handleRead()  ？

**acceptChannel_.readCallBack_ --> Acceptor::handleRead（） -->  Acceptor::newConnectionCallback_ -->  TcpServer::newConnection(connfd, peerAddr)**

该函数是 用来 传递给 acceptChannel_ 的 readCallBack_回调的； 

​		设置 acceptChannel_ 的 readCallBack_ -> Acceptor::handleRead（）

<img src="手写muduo项目.assets/image-20230801200808259.png" alt="image-20230801200808259" style="zoom:67%;" />

功能： 调用 accept（） ，peerAddr  保存传出的 连接的 <客户端> 的地址结构（IP+port），并返回连接的 客户端的 connfd

并 调用 newConnectionCallback_ 回调函数，传入 连接的客户端的 connfd、地址结构（IP+port）

但是 newConnectionCallback_  在哪里设置的？

<img src="手写muduo项目.assets/image-20230801205634133.png" alt="image-20230801205634133" style="zoom: 50%;" />

<img src="手写muduo项目.assets/image-20230801204329562.png" alt="image-20230801204329562" style="zoom: 67%;" />

​		// newConnectionCallback_ 在 TcpServer的构造函数里设置：绑定到TcpServer::newConnection函数

<img src="手写muduo项目.assets/image-20230801201559915.png" alt="image-20230801201559915" style="zoom: 50%;" />



# Buffer

TCP粘包 ： 在通讯数据里加一个数据头和数据长度，根据数据长度来截取包的大小

应用 只需要根据包头的大小来读取一部分，未读取的数据要在缓冲区里存储下来

 发送也是有一样：应用要写的数据比较多，跟socket绑定的TCP缓冲区大小比较小，数据无法一次性发送完成，未发送的数据要在缓冲区里存储下来

![image-20230730104845127](手写muduo项目.assets/image-20230730104845127.png)

#### 读写缓冲区理解

注意：  读写缓冲区 实际上是 **共用的一段连续内存**

这里，这段buffer_数组中全部的元素都有任意初始值，但是通过两个索引 readerIndex_ 和 writerIndex_

[ readerIndex_  ~ writerIndex_ ] 区间的数组元素，保存着从fd上 读取的数据 ，这一区间的数组数据，用来

[ writerIndex_  ~  数组最后一个] 区间的数组元素，是还没有被读取数据覆盖的 数组元素，这一区间的数组元素值不是我们保存的值，这块区间是等待着被我们 读取的值去覆盖的。

![image-20230730140111781](手写muduo项目.assets/image-20230730140111781.png)

#### 构造函数

读写缓冲区开始索引，是相等的，此时读缓冲区长度为0，写缓冲区长度默认为1024字节

![image-20230730112729572](手写muduo项目.assets/image-20230730112729572.png)

#### readFd()   -> 存到缓冲区

作用：调用readv()，将从fd中读取到的数据，存到 缓冲区中。读缓冲区扩大，写缓冲区后移缩小

如果数据 大于写缓冲区剩下的容量，则对写缓冲区扩容，并将数据存入后，将 writerIndex_ 向后移动到相应的位置

![image-20230730114059758](手写muduo项目.assets/image-20230730114059758.png)

#### retrieveAsString（） retrieveAllAsString()

[ readerIndex_  ~ writerIndex_ ] 区间的数组元素，保存着从fd上 读取的数据 

读取该区间的 len个数组元素 ，构造string对象并返回 

读取该区间的 全部数组元素 ，构造string对象并返回 。读取完之后，readerIndex_  、writerIndex_又回到初始索引处

![image-20230730135927255](手写muduo项目.assets/image-20230730135927255.png)

#### writeFd() 

将readerIndex_  ~ writerIndex_ 之间的保存的所有数组元素（也就是读取的数据），写入到 设备fd 中

<img src="手写muduo项目.assets/image-20230730115154542.png" alt="image-20230730115154542" style="zoom:67%;" />

# TcpConnnection （-> TcpServer::newConnection）

重要成员

// 在 TcpServer::newConnection() 中被调用

<img src="手写muduo项目.assets/image-20230802182918740.png" alt="image-20230802182918740" style="zoom:67%;" />

<img src="手写muduo项目.assets/image-20230802183038166.png" alt="image-20230802183038166" style="zoom: 67%;" />

TcpConnection的 4个回调，在 TcpServer::newConnection 刚刚new出来后，就被 赋值了

<img src="手写muduo项目.assets/image-20230802203630789.png" alt="image-20230802203630789" style="zoom: 50%;" />



#### 构造函数

在 TcpServer::newConnection(connfd, peerAddr) 中 new 创建了 对应 **连接客户端 connfd 的 Socket + Channel**

![image-20230801224653483](手写muduo项目.assets/image-20230801224653483.png)

设置了Channel成员的 读写关闭错误  回调函数

Channel::readCallBack_	->   TcpConnection::handleRead

Channel::writeCallBack_	->   TcpConnection::handleWrite

Channel::closeCallBack_	->   TcpConnection::handleClose

Channel::errorCallBack_	->   TcpConnection::handleError

<img src="手写muduo项目.assets/image-20230730171522909.png" alt="image-20230730171522909" style="zoom: 50%;" />



#### state_

构造函数时 ：kConnecting

connectEstablished()：kConnected

shutdown()：kDisconnecting

handleClose() : kDisconnected



#### handleRead（）



# =====================================

# TcpServer（-> 用户）

数据成员



成员

这几个设置Callback的成员函数，是提供给用户，在server中调用的

<img src="手写muduo项目.assets/image-20230801142334844.png" alt="image-20230801142334844" style="zoom: 67%;" />

<img src="手写muduo项目.assets/image-20230801142306063.png" alt="image-20230801142306063" style="zoom:67%;" />

#### 构造函数

#### <img src="手写muduo项目.assets/image-20230801155346510.png" alt="image-20230801155346510" style="zoom:50%;" />

unique_ptr  **acceptor_**  ->  new **创建了一个 Acceptor对象**，将 mainloop 传给了 此Acceptor

shared_ptr  **threadPool_**   ->  new 创建了一个 EventLoopThreadPool对象，将 mainloop 传给了 此EventLoopThreadPool

设置 Acceptor对象 的 

![](手写muduo项目.assets/image-20230801155943662.png)

#### newConnection

创建了 对应 连接客户端的 connfd 的 TcpConnection对象，并设置了 回调函数

**acceptChannel_.readCallBack_ --> Acceptor::handleRead（） -->  Acceptor::newConnectionCallback_ -->  TcpServer::newConnection(connfd, peerAddr)  -->  **

在Acceptor::handleRead（）中会 调用 本函数

![image-20230801205918512](手写muduo项目.assets/image-20230801205918512.png)

<img src="手写muduo项目.assets/image-20230801201559915.png" alt="image-20230801201559915" style="zoom: 50%;" />



![image-20230802183846548](手写muduo项目.assets/image-20230802183846548.png)

将

![](手写muduo项目.assets/image-20230802183703149.png)

# EchoServer（ 用户）

数据成员：

EventLoop类 指针 ：loop_  ->  指向 mainloop 对象

TcpServer类对象 ： server_ ：由传入的 mainloop 对象

<img src="手写muduo项目.assets/image-20230801144836258.png" alt="image-20230801144836258" style="zoom: 67%;" />

#### main()

创建mainloop，

![image-20230801144038902](手写muduo项目.assets/image-20230801144038902.png)

#### 构造函数

初始化2个数据成员：EventLoop *loop_  和 TcpServer server_

重要： 传入  自己实现 的函数，赋值给 TcpServer 的 回调函数成员

**用户 -> TcpServer -> TcpConnnection  -> Channel**  (加入Poller，监听事件发生后，Channel调用相应的回调） 

![image-20230801144642691](手写muduo项目.assets/image-20230801144642691.png)

 TcpServer 提供的接口

![image-20230801231840336](手写muduo项目.assets/image-20230801231840336.png)

<img src="手写muduo项目.assets/image-20230801231834701.png" alt="image-20230801231834701" style="zoom:67%;" />

#### Callback 赋值流程：

用户 -> TcpServer -> TcpConnnection  -> Channel  (加入Poller，监听事件发生后，Channel调用相应的回调） 

