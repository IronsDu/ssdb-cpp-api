#ifndef __SSDB_CLIENT_H__
#define __SSDB_CLIENT_H__

#include <vector>
#include <string>
#include <stdint.h>

class SSDBProtocolResponse;
struct buffer_s;

class Status
{
public:
    Status(){}
    Status(const std::string& code)
    {
        mCode = code;
    }

    int             not_found()
    {
        return mCode == "not_found";
    }

    int             ok()
    {
        return mCode == "ok";
    }

    int             error()
    {
        return mCode != "ok";
    }

    std::string     code()
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

    void                    connect(const char* ip, int port);

    Status                  get(const std::string key, std::string *val);

	Status		            hset(const std::string name, const std::string key, std::string val);

	Status		            hget(const std::string name, const std::string key, std::string *val);

	Status		            zset(const std::string name, const std::string key, int64_t score);

	Status		            zget(const std::string name, const std::string key, int64_t *score);

	Status		            zsize(const std::string name, int64_t *size);

	Status		            zkeys(const std::string name, const std::string key_start, 
		int64_t score_start, int64_t score_end,uint64_t limit, std::vector<std::string> *ret);

	Status		            zscan(const std::string name, const std::string key_start, 
		int64_t score_start, int64_t score_end,uint64_t limit, std::vector<std::string> *ret);

	Status		            zclear(const std::string name);

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

    int                     m_socket;

    std::string             m_ip;
    int                     m_port;
};

#endif
