#include "server/webserver.h"

int
main() {
    WebServer webserver(1316, 3, 60000, 6,            /* 设置端口, ET模式, 超时时间, 线程池线程数量 */
                        3306, "root", "root", "YAWS", /* Mysql 配置 */
                        12, false, true, 1, 1024); /* 连接池数量, 优雅退出, 日志开关, 日志等级, 日志异步队列容量 */
    webserver.Start();
    return 0;
}