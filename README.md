# CGIServer
---
1. 基于半同步/半异步进程池实现，主线程只监听socket，连接socket交由进程池中的进程监听
2. fork()以后内存空间是独立的，所以静态变量也会独一份
3. listenfd是继承过来的，都是监听socket，但是epollfd则是各进程独有
4. 静态非const成员变量只能在类外初始化
