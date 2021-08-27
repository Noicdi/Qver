/*
 * Author: xQmQ
 * Date: 2021/8/22 16:00
 * Description: Epoller 类的实现
 */

#include "../include/epoller.h"
#include <iostream>

Epoller::Epoller()
{
  epoll_fd_ = epoll_create(5);
  assert(epoll_fd_ != -1);
}

int
Epoller::addEvent(int fd) const
{
  struct epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;

  return epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event);
}

int
Epoller::delEvent(int fd) const
{
  int result = epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
  if (result == -1)
    return -1;

  close(fd);

  return result;
}

int
Epoller::getFds(std::vector<int> &get_fds, std::vector<int> &err_fds)
{
  // 清理，防止干扰
  get_fds.clear();
  err_fds.clear();

  int number = epoll_wait(epoll_fd_, events_, MAX_EVENT_NUMBER, 0);

  if (number == -1)
    return -1;

  // 收集可读的文件描述符
  // 关闭 EPOLLERR 状态的文件描述符
  // TODO 可能需要处理其他状态
  for (int i = 0; i < number; ++i)
    // 客户端主动关闭 socket
    // 需要对此做处理
    if (events_[i].events & EPOLLRDHUP) {
      err_fds.push_back(events_[i].data.fd);
      close(events_[i].data.fd);
    } else if (events_[i].events & EPOLLIN)
      get_fds.push_back(events_[i].data.fd);
    else if (events_[i].events & EPOLLHUP) {
      err_fds.push_back(events_[i].data.fd);
      close(events_[i].data.fd);
    }

  return number;
}