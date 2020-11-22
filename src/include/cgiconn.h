//
// Created by Eddie on 2020/11/22.
//

#ifndef CGISERVER_CGICONN_H
#define CGISERVER_CGICONN_H

#include "processpool.h"

class cgi_conn{
public:
    cgi_conn(){};
    ~cgi_conn(){};

    void init(int epollfd, int sockfd, const sockaddr_in& client_addr);
    void process();

private:
    static const int BUFFFER_SIZE = 1024;
    static int m_epollfd;
    int m_sockfd;
    sockaddr_in m_address;
    char m_buf[BUFFFER_SIZE];
    int m_read_idx;
};
int cgi_conn::m_epollfd = -1;

#endif //CGISERVER_CGICONN_H
