#include "webserver.h"

WebServer::WebServer(int port, int trigMode, int timeoutMs, int threadNum, int sqlPort, const char *sqlUser,
                     const char *sqlPwd, const char *dbName, int connPoolNum, bool OptLinger, bool openLog,
                     int logLevel, int logQueSize) :
    port_(port),
    timeoutMs_(timeoutMs), isClose_(false), openLinger_(OptLinger), timer_(new HeapTimer()), epoller_(new Epoller()),
    threadpool_(new ThreadPool(threadNum)) {
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16);
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;

    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    InitEventMode_(trigMode);
    if (!InitSocket_()) { isClose_ = true; }

    if (openLog) {
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if (isClose_) {
            LOG_ERROR("========== Server init error!==========")
        } else {
            LOG_INFO("========== Server init==========");
            LOG_INFO("Port: %d, OpenLinger: %s", port_, OptLinger ? "true" : "false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s", (listenEvent_ & EPOLLET ? "ET" : "LT"),
                     (connEvent_ & EPOLLET ? "ET" : "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("ThreadPool Num: %d", threadNum);
        }
    }
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
}

void
WebServer::Start() {
    int timeMs = -1;
    if (!isClose_) { LOG_INFO("========== Server start =========="); }
    while (!isClose_) {
        if (timeoutMs_ > 0) { timeMs = timer_->GetNextTick(); }
        int eventCnt = epoller_->Wait(timeMs);
        for (int i = 0; i < eventCnt; ++i) {
            int fd = epoller_->GetEventFd(i);         // 获取事件对应的fd
            uint32_t events = epoller_->GetEvents(i); // 获取事件的类型
            if (fd == listenFd_) {
                DealListen_();
            } else if (events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]); // 子线程中执行
            } else if (events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]); // 子线程中执行
            } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void
WebServer::SendError_(int fd, const char *info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) { LOG_WARN("send error to client[%d] error!", fd); }
    close(fd);
}

void
WebServer::CloseConn_(HttpConn *client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

void
WebServer::AddClient(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if (timeoutMs_ > 0) { timer_->add(fd, timeoutMs_, std::bind(&WebServer::CloseConn_, this, &users_[fd])); }
    SetFdNonblock(fd);
    epoller_->AddFd(fd, connEvent_ | EPOLLIN);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

void
WebServer::DealListen_() {
    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);
    do {
        int cfd = accept(listenFd_, (struct sockaddr *)&cliaddr, &len);
        if (cfd <= 0) {
            return;
        } else if (HttpConn::userCount >= MAX_FD) {
            SendError_(cfd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient(cfd, cliaddr);
    } while (listenEvent_ & EPOLLET);
}

void
WebServer::DealRead_(HttpConn *client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

void
WebServer::DealWrite_(HttpConn *client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

void
WebServer::ExtentTime_(HttpConn *client) {
    assert(client);
    if (timeoutMs_ > 0) { timer_->adjust(client->GetFd(), timeoutMs_); }
}

void
WebServer::OnRead_(HttpConn *client) {
    assert(client);
    int ret = -1;
    int readError = 0;
    ret = client->read(&readError);
    if (ret <= 0 && readError != EAGAIN) {
        CloseConn_(client);
        return;
    }
    OnProcess(client);
}

void
WebServer::OnProcess(HttpConn *client) {
    if (client->process()) {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

void
WebServer::OnWrite_(HttpConn *client) {
    assert(client);
    int ret = -1;
    int writeError = 0;
    ret = client->write(&writeError);
    if (client->ToWriteBytes() == 0) {
        if (client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
    } else if (ret < 0) {
        if (writeError == EAGAIN) {
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
            return;
        }
    }
    CloseConn_(client);
}

void
WebServer::InitEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLRDHUP | EPOLLONESHOT; // 注册EPOLLONESHOT，防止多线程同时操作一个socket的情况
    switch ((trigMode)) {
        case 0: break;
        case 1: connEvent_ |= EPOLLET; break;
        case 2: listenEvent_ |= EPOLLET; break;
        case 3:
            connEvent_ |= EPOLLET;
            listenEvent_ |= EPOLLET;
            break;
        default:
            connEvent_ |= EPOLLET;
            listenEvent_ |= EPOLLET;
            break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}

bool
WebServer::InitSocket_() {
    if (port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port: %d error!", port_);
        return false;
    }
    // 创建socket
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        LOG_ERROR("Create socket error!");
        return false;
    }

    // 设置端口复用
    int optval = 1;
    int ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!");
        return false;
    }

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(port_);
    // 绑定
    ret = bind(listenFd_, (struct sockaddr *)&saddr, sizeof(saddr));
    if (ret < 0) {
        LOG_ERROR("Bind Port: %d error!", port_);
        close(listenFd_);
        return false;
    }

    // 监听
    ret = listen(listenFd_, 128);
    if (ret < 0) {
        LOG_ERROR("Listen Port: %d error!", port_);
        close(listenFd_);
        return false;
    }

    // 将listenFd_注册至epoll事件表中
    ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);
    if (ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }

    // 设置listenFd_非阻塞
    SetFdNonblock(listenFd_);
    LOG_INFO("Server Port: %d", port_);
    return true;
}

int
WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}