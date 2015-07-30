#ifndef __SSDB_CLIENT_H__
#define __SSDB_CLIENT_H__

#include <vector>
#include <string>
#include <unordered_map>
#include <stdint.h>

/*  同步ssdb client api   */

class SSDBProtocolResponse;
class SSDBProtocolRequest;

struct buffer_s;

class Status
{
public:
    Status(){}
    Status(const std::string& code)
    {
        mCode = code;
    }

    int             not_found() const
    {
        return mCode == "not_found";
    }

    int             ok() const
    {
        return mCode == "ok";
    }

    int             error() const
    {
        return mCode != "ok";
    }

    std::string     code() const
    {
        return mCode;
    }

private:
    std::string     mCode;
};

class SSDBClient
{
public:
    SSDBClient();
    ~SSDBClient();

    void                    disConnect();
    void                    connect(const char* ip, int port);
    bool                    isConnect() const;

    void                    execute(const char* str, int len);

    Status                  set(const std::string& key, const std::string& val);

    Status                  get(const std::string& key, std::string *val);

    Status                  hset(const std::string& name, const std::string& key, std::string val);
    Status                  multiHset(const std::string& name, const std::unordered_map<std::string, std::string> &kvs);
    Status                  hget(const std::string& name, const std::string& key, std::string *val);
    Status                  multiHget(const std::string& name, const std::vector<std::string> &keys, std::vector<std::string> *ret);

    Status                  zset(const std::string& name, const std::string& key, int64_t score);

    Status                  zget(const std::string& name, const std::string& key, int64_t *score);

    Status                  zsize(const std::string& name, int64_t *size);

    Status                  zkeys(const std::string& name, const std::string& key_start,
                                    int64_t score_start, int64_t score_end,uint64_t limit, std::vector<std::string> *ret);

    Status                  zscan(const std::string& name, const std::string& key_start,
                                    int64_t score_start, int64_t score_end,uint64_t limit, std::vector<std::string> *ret);

    Status                  zclear(const std::string& name);

    Status                  qpush(const std::string& name, const std::string& item);
    Status                  qpop(const std::string& name, std::string* item);
    Status                  qslice(const std::string& name, int64_t begin, int64_t end, std::vector<std::string> *ret);
    Status                  qclear(const std::string& name);

private:
    SSDBClient(const SSDBClient&); 
    void operator=(const SSDBClient&); 

private:
    void                    request(const char*, int len);
    int                     send(const char* buffer, int len);
    void                    recv();

private:
    buffer_s*               m_recvBuffer;
    SSDBProtocolResponse*   m_reponse;
    SSDBProtocolRequest*    m_request;

    int                     m_socket;

    std::string             m_ip;
    int                     m_port;
};

#endif
