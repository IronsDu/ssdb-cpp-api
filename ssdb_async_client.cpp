#include "ssdb_async_client.h"

SSDBAsyncClient::SSDBAsyncClient()
{
    mConnectStatus = SSDB_CONNECT_NONE;
    mCloseDBThread = false;
    mDBThread = nullptr;
}

SSDBAsyncClient::~SSDBAsyncClient()
{
    closeDBThread();
}

void    SSDBAsyncClient::pollDBReply(int ms)
{
    mLogicFunctorMQ.SyncRead(ms);

    std::function<void(void)> tmp;
    while (mLogicFunctorMQ.PopFront(&tmp))
    {
        tmp();
    }
    mDBFunctorMQ.ForceSyncWrite();
}

void SSDBAsyncClient::postStartDBThread(std::string ip, int port)
{
    if (mConnectStatus == SSDB_CONNECT_NONE || mConnectStatus == SSDB_CONNECT_CLOSE)
    {
        waitCloseDBThread();

        mConnectStatus = SSDB_CONNECT_POST;
        mCloseDBThread = false;

        mDBThread = new std::thread([this, ip, port](){
            dbThread(ip, port);
        });
    }
}

SSDBAsyncClient::SSDB_CONNECT_STATUS SSDBAsyncClient::getConnectStatus() const
{
    return mConnectStatus;
}

void SSDBAsyncClient::closeDBThread()
{
    mCloseDBThread = true;
    waitCloseDBThread();
    mConnectStatus = SSDB_CONNECT_NONE;
}

void    SSDBAsyncClient::dbThread(std::string ip, int port)
{
    /*  TODO::·Ç×èÈû   */
    mSSDBClient.connect(ip.c_str(), port);

    while (!mCloseDBThread)
    {
        if (!mSSDBClient.isConnect())
        {
            mConnectStatus = SSDB_CONNECT_CLOSE;
            break;
        }

        mDBFunctorMQ.SyncRead(1);

        std::function<void(void)> proc;
        while (mDBFunctorMQ.PopFront(&proc))
        {
            proc();
        }

        mLogicFunctorMQ.ForceSyncWrite();
    }

    mSSDBClient.disConnect();
}

void SSDBAsyncClient::waitCloseDBThread()
{
    if (mDBThread != nullptr)
    {
        mDBThread->join();
        delete mDBThread;
        mDBThread = nullptr;
    }
}

void    SSDBAsyncClient::set(const std::string& key, const std::string& value, const std::function<void(const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        Status status = mSSDBClient.set(key, value);

        mLogicFunctorMQ.Push([=](){
            callback(status);
        });
    });
}

void    SSDBAsyncClient::get(const std::string& key, const std::function<void(const std::string&, const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        std::string value;
        Status status = mSSDBClient.get(key, &value);

        mLogicFunctorMQ.Push([=](){
            callback(value, status);
        });
    });
}

void    SSDBAsyncClient::hset(const std::string& name, const std::string& key, const std::string& val,
    const std::function<void(const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        Status status = mSSDBClient.hset(name, key, val);

        mLogicFunctorMQ.Push([=](){
            callback(status);
        });
    });
}

void    SSDBAsyncClient::hget(const std::string& name, const std::string& key,
    const std::function<void(const std::string&, const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        std::string value;
        Status status = mSSDBClient.hget(name, key, &value);
        std::string&& tmp = std::move(value);

        mLogicFunctorMQ.Push([=](){
            callback(tmp, status);
        });
    });
}

void    SSDBAsyncClient::zset(const std::string& name, const std::string& key, int64_t score,
    const std::function<void(const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        Status status = mSSDBClient.zset(name, key, score);

        mLogicFunctorMQ.Push([=](){
            callback(status);
        });
    });
}

void    SSDBAsyncClient::zget(const std::string& name, const std::string& key, const std::function<void(int64_t, const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        int64_t score;
        Status status = mSSDBClient.zget(name, key, &score);

        mLogicFunctorMQ.Push([=](){
            callback(score, status);
        });
    });
}

void    SSDBAsyncClient::zsize(const std::string& name, const std::function<void(int64_t, const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        int64_t size;
        Status status = mSSDBClient.zsize(name, &size);

        mLogicFunctorMQ.Push([=](){
            callback(size, status);
        });
    });
}

void    SSDBAsyncClient::zkeys(const std::string& name, const std::string& key_start, int64_t score_start, int64_t score_end,
    uint64_t limit, const std::function<void(const std::vector<std::string>&, const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        std::vector<std::string> ret;
        Status status = mSSDBClient.zkeys(name, key_start, score_start, score_end, limit, &ret);
        std::vector<std::string>&& tmp = std::move(ret);

        mLogicFunctorMQ.Push([=](){
            callback(tmp, status);
        });
    });
}

void    SSDBAsyncClient::zscan(const std::string& name, const std::string& key_start, int64_t score_start, int64_t score_end,
    uint64_t limit, const std::function<void(const std::vector<std::string>&, const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        std::vector<std::string> ret;
        Status status = mSSDBClient.zscan(name, key_start, score_start, score_end, limit, &ret);
        std::vector<std::string>&& tmp = std::move(ret);
        mLogicFunctorMQ.Push([=](){
            callback(tmp, status);
        });
    });
}

void    SSDBAsyncClient::zclear(const std::string& name, const std::function<void(const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        Status status = mSSDBClient.zclear(name);
        mLogicFunctorMQ.Push([=](){
            callback(status);
        });
    });
}