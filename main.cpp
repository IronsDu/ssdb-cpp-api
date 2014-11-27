#include <string>
#include <iostream>
using namespace std;

#include "ssdb_client.h"

int main()
{
    SSDBClient client;
    client.connect("192.168.16.5", 8888);
    Status s = client.hset("haha", "a", "1");
    cout << "hset haha[a]:1 " << ", status:" << s.code() << endl;
    string v;
    s = client.hget("haha", "a", &v);
    cout << "hget haha[a] is " << v << ", status:" << s.code() << endl;
    s = client.hget("haha", "b", &v);
    cout << "hget haha[b] is " << v << ", status:" << s.code() << endl;
    return 0;
}