#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/uio.h>   // readv/writev
#include <arpa/inet.h> // sockaddr_in
#include <stdlib.h>    // atoi()
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <atomic>

#include "../log/log.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"

class HttpConn {
public:
    HttpConn();
    ~HttpConn();
    void init(int sockFd, const sockaddr_in &addr);
    void Close();
    int GetFd() const;
    int GetPort() const;
    const char *GetIp() const;
    sockaddr_in GetAddr() const;

    ssize_t read(int *saveError);
    ssize_t write(int *saveError);

    bool process();

    int ToWriteBytes() { return iov_[0].iov_len + iov_[1].iov_len; }

    bool IsKeepAlive() const { return request_.IsKeepAlive(); }

    static std::atomic<int> userCount;
    static bool isET;
    static const char *srcDir;

private:
    int fd_;
    struct sockaddr_in addr_;

    int iovCnt_;
    struct iovec iov_[2];

    Buffer readBuffer_;
    Buffer writeBuffer_;

    HttpRequest request_;
    HttpResponse response_;

    bool isClose_;
};

#endif // HTTP_CONN_H