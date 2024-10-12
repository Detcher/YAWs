#include "httpconn.h"

const char *HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;

bool HttpConn::isET = true;

HttpConn::HttpConn() {
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
}

HttpConn::~HttpConn() { Close(); }

void
HttpConn::init(int fd, const sockaddr_in &addr) {
    assert(fd > 0);
    fd_ = fd;
    addr_ = addr;
    userCount++;
    writeBuffer_.RetrieveAll();
    readBuffer_.RetrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s : %d) in, userCount: %d", fd_, GetIp(), GetPort());
}

void
HttpConn::Close() {
    if (isClose_ == false) {
        isClose_ = true;
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s : %d) quit, userCount: %d", fd_, GetIp(), GetPort());
    }
}

int
HttpConn::GetFd() const {
    return fd_;
}

int
HttpConn::GetPort() const {
    return addr_.sin_port;
}

const char *
HttpConn::GetIp() const {
    return inet_ntoa(addr_.sin_addr);
}

sockaddr_in
HttpConn::GetAddr() const {
    return addr_;
}

ssize_t
HttpConn::read(int *saveError) {
    ssize_t len = 0;
    do {
        ssize_t tmp = readBuffer_.ReadFd_(fd_, saveError);
        if (tmp <= 0) {
            break;
        } else {
            len += tmp;
        }
    } while (isET);
    return len;
}

ssize_t
HttpConn::write(int *saveError) {
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iovCnt_);
        if (len <= 0) {
            *saveError = errno;
            break;
        }
        if (iov_[0].iov_len + iov_[1].iov_len == 0) {
            break;
        } else if (static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t *)iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if (iov_[0].iov_len) {
                writeBuffer_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        } else {
            iov_[0].iov_base = (uint8_t *)iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            writeBuffer_.Retrieve(len);
        }
    } while (isET || ToWriteBytes() > 10240);
    return len;
}

bool
HttpConn::process() {
    request_.Init();

    if (readBuffer_.ReadableBytes() <= 0) {
        return false;
    } else if (request_.parse(readBuffer_)) {
        LOG_DEBUG("%s", request_.path().c_str());
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
    } else { // parse failed
        response_.Init(srcDir, request_.path(), false, 400);
    }
    response_.MakeResponse(writeBuffer_);

    iov_[0].iov_base = const_cast<char *>(writeBuffer_.Peek());
    iov_[0].iov_len = writeBuffer_.ReadableBytes();
    iovCnt_ = 1;
    if (response_.FileLen() > 0 && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    LOG_DEBUG("filesize: %ld %d, to %d", response_.FileLen(), iovCnt_, ToWriteBytes());
    return true;
}