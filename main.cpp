//
// Created by Eddie on 2020/11/21.
//

#include <iostream>
#include <processpool.h>
#include <cgiconn.h>

using namespace std;

int main(int argc, char* argv[]){

    if (argc <= 2) {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        //return -1;
    }
    const char* ip = "127.0.0.1";//argv[1];
    int port = 8011;//atoi(argv[2]);
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int ret = 0;
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);//ok
    //inet_pton(AF_INET, ip, &servaddr.sin_addr);//使用"127.0.0.1"，实体机windows客户端连接不进来虚拟机
    address.sin_port = htons(port);

    int on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    if(ret == -1) {
        printf("what: %m\n");
        return -1;
    }

    ret = listen(listenfd, 5);
    assert(ret != -1);

    process_pool<cgi_conn>* pool = process_pool<cgi_conn>::create(listenfd, 8);
    if(pool) {
        pool->run();
        delete pool;
    }

    return 0;
}

