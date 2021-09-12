/*
 * Author: xQmQ
 * Date: 2021/8/20 17:39
 * Description: 封装 socket 类的具体实现
 */

#include "../include/socket.h"

Socket::Socket(const char *ip_address, int port, int listen_length)
{
  // 创建非阻塞 socket
  listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  assert(listen_fd_ >= 0);

  // 绑定端口
  memset(&server_address_, 0, sizeof(server_address_));
  server_address_.sin_family = AF_INET;
  inet_pton(AF_INET, ip_address, &server_address_.sin_addr);
  server_address_.sin_port = htons(port);

  // 命名并设置监听队列长度为5
  int result = bind(listen_fd_, (struct sockaddr *)&server_address_, sizeof(server_address_));
  assert(result != -1);
  result = listen(listen_fd_, listen_length);
  assert(result != -1);
}

Socket::~Socket()
{
  close(listen_fd_);
}

int
Socket::accept(sockaddr_in &client_address)
{
  socklen_t client_address_length = sizeof(client_address);
  memset(&client_address, 0, client_address_length);

  int connection_fd = ::accept(listen_fd_, (struct sockaddr *)&client_address, &client_address_length);

  return connection_fd;
}