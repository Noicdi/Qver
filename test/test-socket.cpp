/*
 * Author: xQmQ
 * Date: 2021/8/21 16:13
 * Description: Socket 类的测试
 */

#include "../include/epoller.h"
#include "../include/signaler.h"
#include "../include/socket.h"
#include "../include/thread-pool.h"
#include "../include/timer.h"
#include "../include/http-event.h"

#include <iostream>
#include <unistd.h>

#define BUF_SIZE 1024

Signaler signaler;

void
signalHandler(int signal)
{
  int save_errno = errno;
  int msg = signal;
  send(signaler.pipe_fd_[1], (char *)&msg, 1, 0);
  errno = save_errno;
}

void
addSignal(int signal)
{
  struct sigaction sa;
  memset(&sa, '\0', sizeof(sa));
  sa.sa_handler = signalHandler;
  sa.sa_flags |= SA_RESTART;
  sigfillset(&sa.sa_mask);
  assert(sigaction(signal, &sa, nullptr) != -1);
}

void
timeoutHandler()
{
  alarm(signaler.timeout_);
}

void
print(int fd)
{
  char buffer[BUF_SIZE];
  for (int i = 0; i < 3; i++) {
    memset(&buffer, '\0', BUF_SIZE);
    int result = recv(fd, &buffer, BUF_SIZE - 1, 0);
    std::cout << fd << "接受到" << result << "字节长度的数据" << std::endl;
    std::cout << buffer << std::endl;
  }
  close(fd);
}

void
http(int fd)
{
  HttpEvent http_event(fd);
  http_event.active();
}

int
main()
{
  Socket socket("127.0.0.1", 12345);
  struct sockaddr_in client;
  Epoller epoller;
  struct epoll_event events[MAX_EVENT_NUMBER];
  std::vector<int> get_fds, err_fds, timeout_fds;
  ThreadPool pool;
  Timer timer;

  addSignal(SIGALRM);
  addSignal(SIGTERM);
  timeoutHandler();
  bool trigger_signal = false;

  epoller.addEvent(signaler.pipe_fd_[0], true, false, false);

  while (!pool.isShutdown()) {
    // 接受客户端连接
    int fd = socket.accept(client);
    if (fd != -1) {
      epoller.addEvent(fd, true, false, true);
      timer.push(fd);
    }

    int number = epoller.getFds(events);
    if (number != -1) {
      for (int i = 0; i < number; ++i) {
        if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
          epoller.delEvent(events[i].data.fd, true);
          timer.pop(events[i].data.fd);
        } else if (events[i].events & EPOLLIN) {
          if (events[i].data.fd == signaler.pipe_fd_[0]) {
            trigger_signal = true;
            continue;
          }
          epoller.delEvent(events[i].data.fd, false);
          timer.pop(events[i].data.fd);
          int work_fd = events[i].data.fd;
          pool.submitWork(&http, work_fd);
        }
      }
    }

    // 触发信号，针对特定信号进行特定处理
    if (trigger_signal) {
      char signals[1024];
      int result = recv(signaler.pipe_fd_[0], signals, sizeof(signals), 0);
      if (result > 0)
        for (int i = 0; i < result; ++i)
          switch (signals[i]) {
            case SIGALRM: { // 触发超时信号，获取超时文件描述符并从内核事件表删除并关闭连接
              timer.tick(timeout_fds);
              for (auto timeout_fd: timeout_fds) {
                epoller.delEvent(timeout_fd, true);
              }
              timeoutHandler();
              break;
            }
            case SIGTERM: { // 触发外部关闭程序的信号，关闭线程池
              pool.shutdown();
              break;
            }
          }
      epoller.addEvent(signaler.pipe_fd_[0], true, false, false);
      trigger_signal = false;
    }
  }

  return 0;
}