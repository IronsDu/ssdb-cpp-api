#include <windows.h>
#include <string>
#include <iostream>

using namespace std;
#include "ssdb_async_client.h"

SSDBAsyncClient ssdb;

static void async_callback(const string& value, const Status& status)
{
    ssdb.get("hello", async_callback);

    static int old_time = 0;
    static int total_count = 0;

    total_count++;

    int now_time = GetTickCount();
    if ((now_time - old_time) >= 1000)
    {
        cout << "total:" << total_count << endl;
        total_count = 0;
        old_time = now_time;
    }
}

int main()
{
    cout << "fuck" << endl;
    ssdb.set("hello", "world", [](const Status& status)
    {
    });

    for (int i = 0; i < 1 * 1; ++i)
    {
        ssdb.get("hello", async_callback);
    }

    ssdb.postAsyncDBFunctor([](){
        /*这里在db线程执行*/
        ssdb.postAsyncLogicFunctor([](){
            /*这里在逻辑线程执行*/
        });
    });

    ssdb.get("hello", [](const string& value, const Status& status){
        cout << "test lambda" << endl;
    });

    while (true)
    {
        /*  判断ssdb链接状态，自动重连 */
        if (ssdb.getConnectStatus() != SSDBAsyncClient::SSDB_CONNECT_OK)
        {
            ssdb.postStartDBThread("127.0.0.1", 8888);
        }

        ssdb.ForceSyncRequest();
        ssdb.pollDBReply(100);
    }

    return 0;
}