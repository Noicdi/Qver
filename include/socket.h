/*
 * Author: xQmQ
 * Date: 2021/8/20 17:39
 * Description: 负责建立、监听并接受 socket 连接，并发送给 epoller 处理
 */

#ifndef QVER_INCLUDE_SOCKET_H_
#define QVER_INCLUDE_SOCKET_H_

#include "non-copyable.h"

#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

class Socket : NonCopyable {
public:
  explicit Socket(const char *ip_address, int port, int listen_length = 5);

  ~Socket();

  /*
   * description: 从监听队列中获取一个 socket 连接
   * param: {client_address->用于保存 socket 连接所对应的主机信息}
   * return: {socket 连接的文件描述符,当返回 -1 表示无 socket 连接}
   */
  int accept(struct sockaddr_in &client_address);

private:
  struct sockaddr_in server_address_; // 被监听的地址及端口
  int listen_fd_;                     // 被监听的文件描述符
};

#endif //QVER_INCLUDE_SOCKET_H_
