#include <vector>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "buffer.h"
#include "socketlibtypes.h"

#include "ssdb_client.h"

#ifdef PLATFORM_WINDOWS
#pragma comment(lib,"ws2_32.lib")
#endif

#if defined PLATFORM_WINDOWS
#define snprintf _snprintf 
#endif

#define DEFAULT_SSDBPROTOCOL_LEN 1024

using namespace std;

static void
ox_socket_init(void)
{
#if defined PLATFORM_WINDOWS
    static WSADATA g_WSAData;
    WSAStartup(MAKEWORD(2,2), &g_WSAData);
#endif
}

static void
ox_socket_close(sock fd)
{
#if defined PLATFORM_WINDOWS
    closesocket(fd);
#else
    close(fd);
#endif
}

static sock
ox_socket_connect(const char* server_ip, int port)
{
    struct sockaddr_in server_addr;
    sock clientfd = SOCKET_ERROR;

    ox_socket_init();

    clientfd = socket(AF_INET, SOCK_STREAM, 0);

    if(clientfd != SOCKET_ERROR)
    {
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(server_ip);
        server_addr.sin_port = htons(port);

        while(connect(clientfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0)
        {
            if(EINTR == sErrno)
            {
                continue;
            }
        }
    }

    return clientfd;
}

static int
ox_socket_nodelay(sock fd)
{
    int flag = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
}

static int
ox_socket_block(sock fd)
{
    int err;
    unsigned long ul = false;
#if defined PLATFORM_WINDOWS
    err = ioctlsocket(fd, FIONBIO, &ul);
#else
    err = ioctl(fd, FIONBIO, &ul);
#endif

    return err != SOCKET_ERROR;
}

class SSDBProtocolRequest
{
public:
    SSDBProtocolRequest()
    {
        m_request = ox_buffer_new(DEFAULT_SSDBPROTOCOL_LEN);
    }

    ~SSDBProtocolRequest()
    {
        ox_buffer_delete(m_request);
        m_request = NULL;
    }

    void        appendStr(const char* str)
    {
        int len = strlen(str);
        char lenstr[16];
        int num = snprintf(lenstr, sizeof(len), "%d\n", len);
        appendBlock(lenstr, num);
        appendBlock(str, len);
        appendBlock("\n", 1);
    }

	void		appendInt64(int64_t val)
	{
		char str[30];
		snprintf(str, sizeof(str), "%lld", val);
		appendStr(str);
	}

    void        appendStr(const string& str)
    {
        char len[16];
        int num = snprintf(len, sizeof(len), "%d\n", (int)str.size());
        appendBlock(len, num);
        appendBlock(str.c_str(), str.length());
        appendBlock("\n", 1);
    }

    void        endl()
    {
        appendBlock("\n", 1);
    }

    void        appendBlock(const char* data, int len)
    {
        if (ox_buffer_getwritevalidcount(m_request) < len)
        {
            buffer_s* temp = ox_buffer_new(ox_buffer_getsize(m_request) + len);
            memcpy(ox_buffer_getwriteptr(temp), ox_buffer_getreadptr(m_request), ox_buffer_getreadvalidcount(m_request));
            ox_buffer_addwritepos(temp, ox_buffer_getreadvalidcount(m_request));
            ox_buffer_delete(m_request);
            m_request = temp;
        }

        ox_buffer_write(m_request, data, len);
    }

    const char*     getResult()
    {
        return ox_buffer_getreadptr(m_request);
    }
    int             getResultLen()
    {
        return ox_buffer_getreadvalidcount(m_request);
    }
private:
    buffer_s*   m_request;
};

class SSDBProtocolResponse
{
public:
    ~SSDBProtocolResponse()
    {
        for(int i = 0; i < mBuffers.size(); ++i)
        {
            ox_buffer_delete(mBuffers[i]);
        }
    }

    void                init()
    {
        for(int i = 0; i < mBuffers.size(); ++i)
        {
            ox_buffer_delete(mBuffers[i]);
        }

        mBuffers.clear();
    }

    void                parse(const char* buffer, int len)
    {
        const char* current = buffer;
        while (true)
        {
            char* temp;
            int datasize = strtol(current, &temp, 10);
            current = temp;
            current += 1;
            buffer_s* databuffer = ox_buffer_new(datasize);
            ox_buffer_write(databuffer, current, datasize);
            mBuffers.push_back(databuffer);
            current += datasize;

            current += 1;

            if (*current == '\n')
            {
                /*  收到完整消息,ok  */
                current += 1;         /*  跳过\n    */
                break;
            }
        }
    }

    buffer_s*       getByIndex(int index)
    {
        if(mBuffers.size() > index)
        {
            return mBuffers[index];
        }
        else
        {
            static  const char* nullstr = "null";
            static  buffer_s* nullbuffer = ox_buffer_new(strlen(nullstr)+1);
            static int flag = 0;

            if(flag == 0)
            {
                ox_buffer_write(nullbuffer, nullstr, strlen(nullstr)+1);
            }

            flag = 1;
            return nullbuffer;
        }
    }

	size_t				getBuffersLen() const
	{
		return mBuffers.size();
	}

    Status       getStatus()
    {
        if(mBuffers.empty())
        {
            return Status("error");
        }

        return string(ox_buffer_getreadptr(mBuffers[0]), ox_buffer_getreadvalidcount(mBuffers[0]));
    }

    static int check_ssdb_packet(const char* buffer, int len)
    {
        const char* end = buffer + len; /*  无效内存地址  */
        const char* current = buffer;   /*  当前解析位置*/

        while (true)
        {
            char* temp;
            int datasize = strtol(current, &temp, 10);
            if (datasize == 0 && temp == current)
            {
                break;
            }
            current = temp;         /*  跳过datasize*/

            if (current >= end || *current != '\n')
            {
                break;
            }
            current += 1;         /*  跳过\n    */
            current += datasize;  /*  跳过data  */

            if (current >= end || *current != '\n')
            {
                break;
            }

            current += 1;         /*  跳过\n    */

            if (current >= end)
            {
                break;
            }
            else if(*current == '\n')
            {
                /*  收到完整消息,ok  */
                current += 1;         /*  跳过\n    */
                return (current - buffer);
            }
        }

        /*  非完整消息返回0  */
        return 0;
    }
private:
	
    vector<buffer_s*>   mBuffers;
};

static Status read_list(SSDBProtocolResponse *response, std::vector<std::string> *ret)
{
    Status status = response->getStatus();
    if(status.ok() == 1)
    {
        for(int i = 1; i < response->getBuffersLen(); ++i)
        {
            buffer_s* buffer = response->getByIndex(i);
            ret->push_back(string(ox_buffer_getreadptr(buffer), ox_buffer_getreadvalidcount(buffer)));
        }
    }

	return status;
}

static Status read_int64(SSDBProtocolResponse *response, int64_t *ret)
{
    Status status = response->getStatus();
    if(status.ok() == 1)
    {
        if(response->getBuffersLen() >= 2)
        {
            buffer_s* buf = response->getByIndex(1);
            string temp(ox_buffer_getreadptr(buf), ox_buffer_getreadvalidcount(buf));
            sscanf(temp.c_str(), "%lld",ret);
        }
        else
        {
            status = Status("server_error");
        }
    }
	
	return status;
}

static Status read_str(SSDBProtocolResponse *response, std::string *ret)
{
    Status status = response->getStatus();
    if(status.ok() == 1)
    {
        if(response->getBuffersLen() >= 2)
        {
            buffer_s* buf = response->getByIndex(1);
            *ret = string(ox_buffer_getreadptr(buf), ox_buffer_getreadvalidcount(buf));
        }
        else
        {
            status = Status("server_error");
        }
    }

	return status;
}

void SSDBClient::request(const char* buffer, int len)
{
    int left_len = send(buffer, len);

    /*  如果发送请求完毕，就进行接收response处理    */
    if(len > 0 && left_len == 0)
    {
        recv();
    }

    /*  TODO::重新连接  */
    if(m_socket == SOCKET_ERROR)
    {
        connect(m_ip.c_str(), m_port);
    }
}

int SSDBClient::send(const char* buffer, int len)
{
    int left_len = len;
    while(m_socket != SOCKET_ERROR && left_len > 0)
    {
        int sendret = ::send(m_socket, buffer+(len-left_len), left_len, 0);
        if(sendret < 0)
        {
            if(sErrno != S_EINTR && sErrno != S_EWOULDBLOCK)
            {
                /*  链接断开    */
                ox_socket_close(m_socket);
                m_socket = SOCKET_ERROR;
                break;
            }
        }
        else
        {
            left_len -= sendret;
        }
    }

    return left_len;
}

void SSDBClient::recv()
{
    /*  重置读缓冲区  */
    ox_buffer_init(m_recvBuffer);
    while(m_socket != SOCKET_ERROR)
    {
        if(ox_buffer_getwritevalidcount(m_recvBuffer) < 128)
        {
            /*  扩大缓冲区   */
            buffer_s* temp = ox_buffer_new(ox_buffer_getsize(m_recvBuffer) + 1024);
            memcpy(ox_buffer_getwriteptr(temp), ox_buffer_getreadptr(m_recvBuffer), ox_buffer_getreadvalidcount(m_recvBuffer));
            ox_buffer_addwritepos(temp, ox_buffer_getreadvalidcount(m_recvBuffer));
            ox_buffer_delete(m_recvBuffer);
            m_recvBuffer = temp;
        }

        int len = ::recv(m_socket, ox_buffer_getwriteptr(m_recvBuffer), ox_buffer_getwritevalidcount(m_recvBuffer), 0);
        if((len == -1 && sErrno != S_EINTR && sErrno != S_EWOULDBLOCK) || len == 0)
        {
            ox_socket_close(m_socket);
            m_socket = SOCKET_ERROR;
            break;
        }
        else if(len > 0)
        {
            ox_buffer_addwritepos(m_recvBuffer, len);

            /*  尝试解析,返回值大于0表示接受到完整的response消息包    */
            if(SSDBProtocolResponse::check_ssdb_packet(ox_buffer_getreadptr(m_recvBuffer), ox_buffer_getreadvalidcount(m_recvBuffer)) > 0)
            {
                m_reponse->init();
                m_reponse->parse(ox_buffer_getreadptr(m_recvBuffer), ox_buffer_getreadvalidcount(m_recvBuffer));
                break;
            }
        }
    }
}

SSDBClient::SSDBClient()
{
    ox_socket_init();
    m_reponse = new SSDBProtocolResponse;
    m_socket = SOCKET_ERROR;
    m_recvBuffer = ox_buffer_new(DEFAULT_SSDBPROTOCOL_LEN);
}

SSDBClient::~SSDBClient()
{
    if(m_socket != SOCKET_ERROR)
    {
        ox_socket_close(m_socket);
        m_socket = SOCKET_ERROR;
    }
    if(m_reponse != NULL)
    {
        delete m_reponse;
        m_reponse = NULL;
    }
    if(m_recvBuffer != NULL)
    {
        ox_buffer_delete(m_recvBuffer);
        m_recvBuffer = NULL;
    }

#if defined PLATFORM_WINDOWS
    WSACleanup();
#endif
}

void SSDBClient::connect(const char* ip, int port)
{
    if(m_socket == SOCKET_ERROR)
    {
        m_socket = ox_socket_connect(ip, port);
        if(m_socket != SOCKET_ERROR)
        {
            ox_socket_nodelay(m_socket);
            ox_socket_block(m_socket);
        }

        m_ip = ip;
        m_port = port;
    }
}

Status SSDBClient::get(const std::string key, std::string *val)
{
    SSDBProtocolRequest temp;
    temp.appendStr("get");
    temp.appendStr(key);
    temp.endl();

    request(temp.getResult(), temp.getResultLen());

    return read_str(m_reponse, val);
}

Status SSDBClient::hset(const std::string name, const std::string key, std::string val)
{
	SSDBProtocolRequest temp;
	temp.appendStr("hset");
	temp.appendStr(name);
	temp.appendStr(key);
	temp.appendStr(val);
	temp.endl();

	request(temp.getResult(), temp.getResultLen());

	return m_reponse->getStatus();
}

Status SSDBClient::hget(const std::string name, const std::string key, std::string *val)
{
	SSDBProtocolRequest temp;
	temp.appendStr("hget");
	temp.appendStr(name);
	temp.appendStr(key);
	temp.endl();

	request(temp.getResult(), temp.getResultLen());

	return read_str(m_reponse, val);
}

Status SSDBClient::zset(const std::string name, const std::string key, int64_t score)
{
	SSDBProtocolRequest temp;
	temp.appendStr("zset");
	temp.appendStr(name);
	temp.appendStr(key);
	char s_str[30];
	sprintf(s_str, "%lld", score);
	temp.appendStr(s_str);
	temp.endl();

	request(temp.getResult(), temp.getResultLen());

	return m_reponse->getStatus();
}

Status SSDBClient::zget(const std::string name, const std::string key, int64_t *score)
{
	SSDBProtocolRequest temp;
	temp.appendStr("zget");
	temp.appendStr(name);
	temp.appendStr(key);
	temp.endl();

	request(temp.getResult(), temp.getResultLen());

	return read_int64(m_reponse, score);
}

Status SSDBClient::zsize(const std::string name, int64_t *size)
{
	SSDBProtocolRequest temp;
	temp.appendStr("zsize");
	temp.appendStr(name);
	temp.endl();

	request(temp.getResult(), temp.getResultLen());

	return read_int64(m_reponse, size);
}

Status SSDBClient::zkeys(const std::string name, const std::string key_start,
	int64_t score_start, int64_t score_end,uint64_t limit, std::vector<std::string> *ret)
{
	SSDBProtocolRequest temp;
	temp.appendStr("zkeys");
	temp.appendStr(name);
	temp.appendStr(key_start);
	temp.appendInt64(score_start);
	temp.appendInt64(score_end);

	char buf[30] = {0};
	snprintf(buf, sizeof(buf), "%llu", limit);
	temp.appendStr(buf);

	temp.endl();

	request(temp.getResult(), temp.getResultLen());

	return read_list(m_reponse, ret);
}

Status SSDBClient::zscan(const std::string name, const std::string key_start,
	int64_t score_start, int64_t score_end,uint64_t limit, std::vector<std::string> *ret)
{
	SSDBProtocolRequest temp;
	temp.appendStr("zscan");
	temp.appendStr(name);
	temp.appendStr(key_start);
	temp.appendInt64(score_start);
	temp.appendInt64(score_end);

	char buf[30] = {0};
	snprintf(buf, sizeof(buf), "%llu", limit);
	temp.appendStr(buf);

	temp.endl();

	request(temp.getResult(), temp.getResultLen());

	return read_list(m_reponse, ret);
}

Status SSDBClient::zclear(const std::string name)
{
	SSDBProtocolRequest temp;
	temp.appendStr("zclear");
	temp.appendStr(name);
	temp.endl();

	request(temp.getResult(), temp.getResultLen());

	return m_reponse->getStatus();
}