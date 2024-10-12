#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <stdio.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <memory>
#include <unordered_map>

#include "epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../pool/threadpool.h"
#include "../http/httpconn.h"
#include "../pool/sqlconnpool.h"
#include "../pool/sqlconnRAII.h"

class WebServer {
public:
    WebServer(int port, int trigMode, int timeoutMs, int threadNum, int sqlPort, const char *sqlUser,
              const char *sqlPwd, const char *dbName, int connPoolNum, bool OptLinger, bool openLog, int logLevel,
              int logQueSize);
    ~WebServer();
    void Start();

private:
    bool InitSocket_();
    void InitEventMode_(int trigMode);
    void AddClient(int fd, sockaddr_in addr);
    static int SetFdNonblock(int fd);

    void SendError_(int fd, const char *info);
    void ExtentTime_(HttpConn *client);

    void DealListen_();
    void DealRead_(HttpConn *client);
    void DealWrite_(HttpConn *client);
    void OnRead_(HttpConn *client);
    void OnWrite_(HttpConn *client);
    void OnProcess(HttpConn *client);
    void CloseConn_(HttpConn *client);

    static const int MAX_FD = 65536;

    int port_;
    int timeoutMs_;
    bool isClose_;
    bool openLinger_;
    int listenFd_;
    char *srcDir_;

    uint32_t listenEvent_;
    uint32_t connEvent_;

    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<Epoller> epoller_;
    std::unique_ptr<ThreadPool> threadpool_;
    std::unordered_map<int, HttpConn> users_;
};

#endif // WEBSERVER_H