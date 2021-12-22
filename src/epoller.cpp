/*
 * Author: xQmQ
 * Date: 2021/8/22 16:00
 * Description: Epoller 类的实现
 */

#include "../include/epoller.h"

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
  struct epoll_event event {
  };
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
  struct epoll_event event {
  };
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
Epoller::getFds(epoll_event *events)
{
  return epoll_wait(epoll_fd_, events, MAX_EVENT_NUMBER, 0);
}
