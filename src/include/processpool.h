//
// Created by Eddie on 2020/11/21.
//

#ifndef CGISERVER_PROCESSPOOL_H
#define CGISERVER_PROCESSPOOL_H

#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <strings.h>
#include <netinet/in.h>
#include <signal.h>
#include <iostream>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>
#include <bits/sigaction.h>

class process{
public:
    pid_t pid;
    int pipefd[2];
};

template <typename T>
class process_pool{
public:
    void run();
public:
    static process_pool<T>* create(int listen_fd, int process_num){
        if(!instance){
            instance = new process_pool(listen_fd, process_num);
        }
        return instance;
    }

private:
    static const int MAX_PROCESS_NUMBER = 16;
    static const int USER_PER_PROCESS = 65536;
    static const int MAX_EVENT_NUMBER = 10000;

    int process_number;
    int index;
    int m_epollfd;
    int listenfd;
    int stop;
    process* sub_process{nullptr};

private:
    void setup_sig_pipe();
    void run_parent();
    void run_child();

private:
    static process_pool<T>* instance;
    process_pool(int listenfd, int pro_num = 8);

};
template <typename T> process_pool<T>* process_pool::instance = nullptr;
//the pipe used to sed signals to main process
static int sig_pipefd[2];

static int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

static void addfd(int epollfd, int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

static void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

static void sig_handler(int sig){
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

static void addsig(int sig, void(handler)(int), bool restart=true){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL)!=-1);
}
#endif //CGISERVER_PROCESSPOOL_H
