#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <fstream>

int
main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in seraddr;
    inet_pton(AF_INET, "127.0.0.1", &seraddr.sin_addr.s_addr);
    seraddr.sin_family = AF_INET;
    seraddr.sin_port = htons(1316);
    // 连接服务器
    int ret = connect(fd, (struct sockaddr *)&seraddr, sizeof(seraddr));
    if (ret == -1) {
        perror("connect");
        return -1;
    }
    std::ifstream t;
    t.open("../test/case1.txt");
    t.seekg(0, std::ios::end);
    int Length = t.tellg();
    t.seekg(0, std::ios::beg);

    char sendBuf[Length] = { 0 };

    t.read(sendBuf, Length);
    t.close();

    write(fd, sendBuf, Length + 1);

    close(fd);
    return 0;
}