# ssdb 跨平台客户端 api
------

### 简介

    
本项目提供`ssdb`的`sync client api`和`async client api` （异步回调）。

`sync client api`由`SSDBClient`类提供 ([`ssdb_client.h`](https://github.com/IronsDu/ssdb-cpp-api/blob/master/ssdb_client.h))。
    
`async client api`由`SSDBAsyncClient`类提供([`ssdb_async_client.h`](https://github.com/IronsDu/ssdb-cpp-api/blob/master/ssdb_async_client.h)) 。
    
> #### **NOTES:**
> 1. 所有接口都非线程安全.
> 2. SSDBClient::connect是阻塞模式.
> 3. 看完下面的API说明后可详细查阅 [`main.cpp`](https://github.com/IronsDu/ssdb-cpp-api/blob/master/main.cpp) 

---


### API 说明
1. Sync Client API

    `SSDBClient::connect(const char* ip, int port)` ： (阻塞)连接ip和port指定的ssdb服务器
    `SSDBClient::disConnect` ： 断开与ssdb服务器的连接
    `SSDBClient::isConnect` ： 获取当前ssdb client与ssdb             server是否连接。返回值类型是bool，true表示已连接，false表示连接断开。

    *其他SSDBClient 命令相关接口与ssdb官方api一致。*

2. Async Client API

    `SSDBAsyncClient::postStartDBThread(std::string ip, int port)`：(投递连接ssdb server)开启ssdb db线程(在其中接收逻辑线程的db请求，调用相关`SSDBClient sync api`接口)
    
    `SSDBAsyncClient::getConnectStatus`：获取当前ssdb client与ssdb server的链接状态.其返回值为`SSDB_CONNECT_STATUS`枚举类型。逻辑线程可以根据此返回值决定是否断线重连。
    
    `SSDBAsyncClient::closeDBThread`：关闭ssdb db thread。
    
    `SSDBAsyncClient::pollDBReply(int ms)`：逻辑线程处理db反馈消息，因为此版本为异步回调接口，所以其会执行db执行某ssdb操作后投递到逻辑线程队列的完成通知（回调）。

    *其他ssdb命令相关接口与官方版本一致，只是多了一个参数：仿函数对象;当db线程处理完某ssdb 操作后会投递完成通知，接下来逻辑线程调用`pollDBReply`后此仿函数对象就会被执行。*




