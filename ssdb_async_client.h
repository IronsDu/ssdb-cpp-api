#ifndef SSDB_ASYNC_CLIENT_H
#define SSDB_ASYNC_CLIENT_H

#include <string>
#include <thread>

#include "ssdb_client.h"
#include "msgqueue.h"

/*  异步ssdb客户端，接口非线程安全 */
class SSDBAsyncClient
{
public:
    enum SSDB_CONNECT_STATUS
    {
        SSDB_CONNECT_NONE,
        SSDB_CONNECT_POST,  /*  已投递开启db线程(开始链接ssdb服务器)    */
        SSDB_CONNECT_OK,    /*  链接成功    */
        SSDB_CONNECT_CLOSE, /*  链接断开    */
    };

public:
    SSDBAsyncClient();
    ~SSDBAsyncClient();

    /*  强制刷新请求  */
    void                    ForceSyncRequest();
    /*  逻辑线程处理db结果  */
    void                    pollDBReply(int ms);

    /*  开启db线程  */
    void                    postStartDBThread(std::string ip, int port, std::function<void(void)> frameCallback = nullptr);
    /*  关闭db线程  */
    void                    closeDBThread();
    SSDB_CONNECT_STATUS     getConnectStatus() const;

    /*  投递异步仿函数到DB线程执行    */
    void                    postAsyncDBFunctor(const std::function<void(void)>& functor);
    /*  投递异步仿函数到主线程执行   */
    void                    postAsyncLogicFunctor(const std::function<void(void)>& functor);

    SSDBClient&             getSyncSSDBClient();

    void                    set(const std::string& key, const std::string& value, const std::function<void(const Status&)>& callback);
    void                    get(const std::string& key, const std::function<void(const std::string&, const Status&)>& callback);

    void                    hset(const std::string& name, const std::string& key, const std::string& val,
                                const std::function<void(const Status&)>& callback);

    void                    hget(const std::string& name, const std::string& key,
                                const std::function<void(const std::string&, const Status&)>& callback);
    void                    multiHget(const std::string& name, const std::vector<std::string>& keys,
                                      const std::function<void(const std::vector<std::string>& values, const Status&)>& callback);
    void                    multiHset(const std::string& name, const std::unordered_map<std::string, std::string>& kvs,
                                      const std::function<void(const Status&)>& callback);

    void                    zset(const std::string& name, const std::string& key, int64_t score,
                                const std::function<void(const Status&)>& callback);

    void                    zget(const std::string& name, const std::string& key, const std::function<void(int64_t, const Status&)>& callback);

    void                    zsize(const std::string& name, const std::function<void(int64_t, const Status&)>& callback);

    void                    zkeys(const std::string& name, const std::string& key_start, int64_t score_start, int64_t score_end,
                                uint64_t limit, const std::function<void(const std::vector<std::string>&, const Status&)>& callback);

    void                    zscan(const std::string& name, const std::string& key_start, int64_t score_start, int64_t score_end,
                                uint64_t limit, const std::function<void(const std::vector<std::string>&, const Status&)>& callback);

    void                    zclear(const std::string& name, const std::function<void(const Status&)>& callback);

    void                    qpush(const std::string& name, const std::string& item, const std::function<void(const Status&)>& callback);
    void                    qpop(const std::string& name, const std::function<void(const std::string&, const Status&)>& callback);
    void                    qslice(const std::string& name, int64_t begin, int64_t end, const std::function<void(const std::vector<std::string>& values, const Status&)>& callback);
    void                    qclear(const std::string& name, const std::function<void(const Status&)>& callback);

private:
    void                    dbThread(std::string ip, int port, std::function<void(void)> frameCallback);
    void                    waitCloseDBThread();
private:
    SSDB_CONNECT_STATUS                     mConnectStatus;
    bool                                    mCloseDBThread;

    std::thread*                            mDBThread;
    MsgQueue<std::function<void(void)>>     mDBFunctorMQ;
    MsgQueue<std::function<void(void)>>     mLogicFunctorMQ;

    SSDBClient                              mSSDBClient;
};

#endif