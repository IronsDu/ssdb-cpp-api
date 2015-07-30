#include <memory>
#include <windows.h>
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

void SSDBAsyncClient::ForceSyncRequest()
{
    mDBFunctorMQ.ForceSyncWrite();
}

void    SSDBAsyncClient::pollDBReply(int ms)
{
    mLogicFunctorMQ.SyncRead(ms);

    std::function<void(void)> tmp;
    while (mLogicFunctorMQ.PopFront(&tmp))
    {
        tmp();
    }
}

void SSDBAsyncClient::postStartDBThread(std::string ip, int port, std::function<void(void)> frameCallback)
{
    if (mConnectStatus == SSDB_CONNECT_NONE || mConnectStatus == SSDB_CONNECT_CLOSE)
    {
        waitCloseDBThread();

        mConnectStatus = SSDB_CONNECT_POST;
        mCloseDBThread = false;

        mDBThread = new std::thread([this, ip, port, frameCallback](){
            dbThread(ip, port, frameCallback);
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

void    SSDBAsyncClient::dbThread(std::string ip, int port, std::function<void(void)> frameCallback)
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

        mLogicFunctorMQ.ForceSyncWrite();
        if (frameCallback != nullptr)
        {
            frameCallback();
        }

        mDBFunctorMQ.SyncRead(100);

        std::function<void(void)> proc;
        while (mDBFunctorMQ.PopFront(&proc))
        {
            proc();
        }
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

void SSDBAsyncClient::postAsyncDBFunctor(const std::function<void(void)>& functor)
{
    mDBFunctorMQ.Push([=](){
        functor();
    });
}

void SSDBAsyncClient::postAsyncLogicFunctor(const std::function<void(void)>& functor)
{
    mLogicFunctorMQ.Push([=](){
        functor();
    });
}

SSDBClient& SSDBAsyncClient::getSyncSSDBClient()
{
    return mSSDBClient;
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
        auto tmp = std::make_shared<std::string>();
        Status status = mSSDBClient.get(key, &(*tmp));

        mLogicFunctorMQ.Push([=](){
            callback(*tmp, status);
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
        auto value = std::make_shared<std::string>();
        Status status = mSSDBClient.hget(name, key, &(*value));

        mLogicFunctorMQ.Push([=](){
            callback(*value, status);
        });
    });
}

void SSDBAsyncClient::multiHget(const std::string& name, const std::vector<std::string>& keys, 
                                const std::function<void(const std::vector<std::string>& values, const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        auto value = std::make_shared<std::vector<std::string>>();
        Status status = mSSDBClient.multiHget(name, keys, &(*value));

        mLogicFunctorMQ.Push([=](){
            callback(*value, status);
        });
    });
}

void SSDBAsyncClient::multiHset(const std::string& name, const std::unordered_map<std::string, std::string>& kvs, const std::function<void(const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        Status status = mSSDBClient.multiHset(name, kvs);

        mLogicFunctorMQ.Push([=](){
            callback(status);
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
        auto ret = std::make_shared<std::vector<std::string>>();
        Status status = mSSDBClient.zkeys(name, key_start, score_start, score_end, limit, &(*ret));

        mLogicFunctorMQ.Push([=](){
            callback(*ret, status);
        });
    });
}

void    SSDBAsyncClient::zscan(const std::string& name, const std::string& key_start, int64_t score_start, int64_t score_end,
    uint64_t limit, const std::function<void(const std::vector<std::string>&, const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        auto ret = std::make_shared<std::vector<std::string>>();
        Status status = mSSDBClient.zscan(name, key_start, score_start, score_end, limit, &(*ret));
        mLogicFunctorMQ.Push([=](){
            callback(*ret, status);
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

void SSDBAsyncClient::qpush(const std::string& name, const std::string& item, const std::function<void(const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        Status status = mSSDBClient.qpush(name, item);

        mLogicFunctorMQ.Push([=](){
            callback(status);
        });
    });
}

void SSDBAsyncClient::qpop(const std::string& name, const std::function<void(const std::string&, const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        auto value = std::make_shared<std::string>();
        Status status = mSSDBClient.qpop(name, value.get());

        mLogicFunctorMQ.Push([=](){
            callback(*value, status);
        });
    });
}

void SSDBAsyncClient::qslice(const std::string& name, int64_t begin, int64_t end, const std::function<void(const std::vector<std::string>& values, const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        auto ret = std::make_shared<std::vector<std::string>>();
        Status status = mSSDBClient.qslice(name, begin, end, &(*ret));

        mLogicFunctorMQ.Push([=](){
            callback(*ret, status);
        });
    });
}

void SSDBAsyncClient::qclear(const std::string& name, const std::function<void(const Status&)>& callback)
{
    mDBFunctorMQ.Push([=](){
        Status status = mSSDBClient.qclear(name);
        mLogicFunctorMQ.Push([=](){
            callback(status);
        });
    });
}