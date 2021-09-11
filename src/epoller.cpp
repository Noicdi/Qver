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

Epoller::~Epoller()
{
  close(epoll_fd_);
}

int
Epoller::addEvent(int fd, bool IN, bool OUT, bool ONESHOT) const
{
  struct epoll_event event{};
  event.data.fd = fd;
  event.events = EPOLLET | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
  if (IN)
    event.events |= EPOLLIN;
  if (OUT)
    event.events |= EPOLLOUT;
  if (ONESHOT)
    event.events |= EPOLLONESHOT;

  return epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event);
}

int
Epoller::modEvent(int fd, int mod_event) const
{
  struct epoll_event event{};
  event.data.fd = fd;
  event.events = EPOLLET | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
  event.events |= mod_event;

  return epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &event);
}

int
Epoller::delEvent(int fd, bool CLOSE) const
{
  int result = epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
  if (result == -1)
    return -1;

  if (CLOSE)
    close(fd);

  return result;
}

int
Epoller::getFds(std::vector<int> *in_fds, std::vector<int> *out_fds, std::vector<int> *err_fds)
{
  // 清理，防止干扰
  if (in_fds)
    in_fds->clear();
  if (out_fds)
    out_fds->clear();
  if (err_fds)
    err_fds->clear();

  struct epoll_event events[MAX_EVENT_NUMBER]{}; // epoll 检测到的事件集

  int number = epoll_wait(epoll_fd_, events, MAX_EVENT_NUMBER, 0);

  if (number == -1)
    return -1;

  // 收集可读、可写的文件描述符
  // 关闭 EPOLLERR 状态的文件描述符
  for (int i = 0; i < number; ++i)
    // 客户端主动关闭 socket
    // 需要对此做处理
    if (events[i].events & EPOLLRDHUP) {
      if (err_fds)
        // 如果需要获得 err_fd，则提供 err_fds 来做容器
        err_fds->push_back(events[i].data.fd);
      // 报错的 fd，无论如何都需要 close()
      delEvent(events[i].data.fd, true);
    } else if ((events[i].events & EPOLLIN) && in_fds) {
      // 存在读就绪事件且需要取出
      in_fds->push_back(events[i].data.fd);
      delEvent(events[i].data.fd, false);
    } else if ((events[i].events & EPOLLOUT) && out_fds) {
      // 存在写就绪事件且需要取出
      out_fds->push_back(events[i].data.fd);
      delEvent(events[i].data.fd, false);
    } else if (events[i].events & EPOLLHUP | EPOLLERR) {
      if (err_fds)
        err_fds->push_back(events[i].data.fd);
      delEvent(events[i].data.fd, true);
    }

  return number;
}
