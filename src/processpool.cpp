//
// Created by Eddie on 2020/11/22.
//

#include "processPool.h"

template <typename T>
process_pool<T>::process_pool(int listenfd, int pro_num)
        :listenfd(listenfd), process_number(pro_num), index(-1), stop(false){
    assert((pro_num>0)&&(pro_num<=MAX_PROCESS_NUMBER));

    sub_process = new process_pool[pro_num];
    assert(sub_process!= nullptr);

    for(int i=0;i<pro_num;i++){
        int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sub_process[i].pipefd);
        assert(ret!=-1);

        sub_process[i].pid = fork();
        if(sub_process[i].pid>0){
            close(sub_process[i].pipefd[1]);
            continue;
        } else{
            close(sub_process[i].pipefd[0]);
            index = i;
            //sub process must break or it will fork
            break;
        }
    }
}

template <typename T>
void process_pool<T>::setup_sig_pipe() {
    //create epoll for every process
    //不同的进程使用不同的地址空间，子进程被创建后，
    //父进程的全局变量，静态变量复制到子进程的地址空间
    //这些变量将相互独立
    m_epollfd = epoll_create(5);
    assert(m_epollfd!=-1);

    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret!=-1);

    setnonblocking(sig_pipefd[1]);
    addfd(m_epollfd, sig_pipefd[0]);

    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, SIG_IGN);
}

template <typename T>
void process_pool<T>::run(){
    if(index!=-1){
        run_child();
        return;
    }
    run_parent();
}

template<typename T>
void process_pool<T>::run_child() {
    setup_sig_pipe();

    int pipefd = sub_process[index].pipefd[1];
    //只需要添加管道，接收主进程消息即可
    addfd(m_epollfd, pipefd);

    epoll_event events[MAX_EVENT_NUMBER];
    T* users = new T[USER_PER_PROCESS];

    int number = 0;
    int ret = -1;

    while(!stop){
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if((number<0)&&(errno!=EINTR)){
            printf("epoll failure\n");
            break;
        }

        for(int i=0;i<number;i++){
            int sockfd = events[i].data.fd;

            if((sockfd==pipefd)&&(events[i].events & EPOLLIN)){
                int client = 0;
                ret = recv(sockfd, (char*)&client, sizeof(client), 0);
                //recv int "1"
                if(((ret<0)&&(errno!=EAGAIN))||ret==0){
                    printf("errno is %d\n", errno);
                    continue;
                }else{
                    struct sockaddr_in client_address;
                    socklen_t  len = sizeof(sockaddr_in);
                    int connfd = accept(listenfd, (struct sockaddr*)&client_address, &len);

                    if(connfd<0){
                        printf("errno is %d\n", errno);
                        continue;
                    }
                    addfd(m_epollfd, connfd);
                    users[connfd].init(m_epollfd, connfd, client_address);
                }
            }else if((sockfd==sig_pipefd[0])&&(events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if(ret <= 0) {
                    continue;
                }
                else {
                    for(int i=0; i<ret; ++i) {
                        switch(signals[i]) {
                            case SIGCHLD: {
                                pid_t pid;
                                int stat;
                                while((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
                                    continue;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT: {
                                stop = true;
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
            }
            else if(events[i].events & EPOLLIN){
                users[sockfd].process();
            } else{
                continue;
            }
        }
    }
    delete []users;
    users = NULL;
    close(pipefd);
    close(m_epollfd);
}

template<typename T>
void process_pool<T>::run_parent() {
    setup_sig_pipe();

    //只有主进程需要添加监听socket
    addfd(m_epollfd, listenfd);
    epoll_event events[MAX_EVENT_NUMBER];
    int sub_processcounter = 0;
    int new_conn = 1;
    int number = 0;
    int ret = -1;

    while(!stop){
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if(((ret<0)&&(errno!=EAGAIN))||ret==0){
            printf("errno is %d\n", errno);
            continue;
        }
        for(int i=0;i<number;i++){
            int sockfd = events[i].data.fd;
            if(sockfd==listenfd){
                int i=sub_processcounter;
                do{
                    if(sub_process[i].pid!=-1){
                        break;
                    }
                    i = (i+1)%process_number;
                }while(i!=sub_processcounter);

                if(sub_process[i].pid == -1){
                    stop = true;
                    break;
                }
                sub_processcounter = (i+1)%process_number;
                int new_conn = 1;
                send(sub_process[i].pipefd[0], (char*)&new_conn, sizeof(new_conn), 0);
                printf("send_request to child:%d\n", i);
            }
            else if((sockfd==sig_pipefd[0])&&(events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                if(ret <= 0)
                    continue;
                else {
                    for(int i=0; i<ret; ++i) {
                        switch(signals[i]) {
                            case SIGCHLD: {
                                pid_t pid;
                                int stat;
                                while((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
                                    for(int i=0; i<process_number; ++i) {
                                        //如果进程池中第i个子进程退出了，则主进程关闭相应的通信管道，并设置响应的pid_位-1，以标记该子进程已经退出
                                        if(sub_process[i].pid_ == pid) {
                                            printf("child %d join\n", i);
                                            close(sub_process[i].pipefd_[0]);
                                            sub_process[i].pid_ = -1;  //标记为-1
                                        }
                                    }
                                }
                                //如果所有的子进程都退出了，则父进程也退出
                                sub_process = true;
                                for(int i=0; i<process_number; ++i) {
                                    if(sub_process[i].pid_ != -1)
                                        stop = false;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT: {
                                //如果父进程收到终止信号，那么就杀死所有子进程，并等待它们全部结束。当然，通知子进程结束的更好
                                //方法是向父子进程之间的通信管道发送特殊数据
                                printf("kill all the child now\n");
                                for(int i=0; i<process_number; ++i) {
                                    int pid = sub_process[i].pid_;
                                    if(pid != -1) {
                                        kill(pid, SIGTERM);
                                    }
                                }
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
            }
            else{
                continue;
            }

        }
    }
    close(m_epollfd);
}
