//
// Created by Eddie on 2020/11/22.
//

#include "cgiconn.h"

void cgi_conn::init(int epollfd, int sockfd, const sockaddr_in &client_addr) {
    m_epollfd = epollfd;
    m_sockfd = sockfd;
    m_address = client_addr;
    memset(m_buf, '\0', BUFFFER_SIZE);
    m_read_idx = 0;
}

void cgi_conn::process() {
    int idx = 0;
    int ret = -1;
    while(true) {
        idx = m_read_idx;
        ret = recv(m_sockfd, m_buf + idx, BUFFFER_SIZE - 1 - idx, 0);
        if (ret < 0) {
            if (errno != EAGAIN) {
                removefd(m_epollfd, m_sockfd);
            }
            break;
        } else if (ret == 0) {
            removefd(m_epollfd, m_sockfd);
            break;
        } else {
            m_read_idx += ret;
            printf("user content is:%s\n", m_buf);
            //如果遇到字符CRLF,则开始处理客户请求
            for (; idx < m_read_idx; idx++) {
                if ((idx >= 1) && (m_buf[idx - 1] == '\r') && (m_buf[idx] == '\n'))
                    //这里查找CRLF采用简单遍历已读数据的方法
                    break;
            }

            if (idx == m_read_idx) {
                continue;
            }
            m_buf[idx - 1] = '\0';

            char *file_name = m_buf;
            printf("file_name=%s\n", file_name);

            //判断客户要运行的CGI程序是否存在
            if (access(file_name, F_OK) == -1) {
                removefd(m_epollfd, m_sockfd);   //不存在就不连接了
                printf("file not found\n");
                break;
            }

            //创建子进程来执行CGI程序
            ret = fork();
            if (ret == -1) {
                removefd(m_epollfd, m_sockfd);
                break;
            } else if (ret > 0) {
                //父进程只需关闭连接
                removefd(m_epollfd, m_sockfd);
                break;   //父进程break
            } else {
                //子进程将标准输出定向到sockfd_,并执行CGI程序
                close(STDOUT_FILENO);
                dup(m_sockfd);
                execl(m_buf, m_buf, 0);
                exit(0);
            }


        }
    }
}



