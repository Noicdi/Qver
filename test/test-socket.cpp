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
    std::cout << "接受到" << result << "字节长度的数据" << std::endl;
    std::cout << buffer << std::endl;
  }
  close(fd);
}

int
main()
{
  Socket socket("127.0.0.1", 12345);
  struct sockaddr_in client;
  Epoller epoller;
  std::vector<int> get_fds, err_fds, timeout_fds;
  ThreadPool pool;
  Timer timer;

  addSignal(SIGALRM);
  addSignal(SIGTERM);
  timeoutHandler();
  bool trigger_signal = false;

  epoller.addEvent(signaler.pipe_fd_[0]);

  while (!pool.isShutdown()) {
    // 接受客户端连接
    int fd = socket.accept(client);
    if (fd != -1) {
      std::cout << "socket:" << fd << std::endl;
      std::cout << "ip:" << inet_ntoa(client.sin_addr) << std::endl;
      std::cout << "port:" << client.sin_port << std::endl;
      epoller.addEvent(fd);
      timer.push(fd);
    }

    // 获取就绪事件
    if (epoller.getFds(get_fds, err_fds) != -1) {
      // 对就绪可读推入任务队列
      for (auto get_fd : get_fds) {
        if (get_fd == signaler.pipe_fd_[0]) {
          std::cout << "触发信号" << std::endl;
          trigger_signal = true;
          continue;
        }
        std::cout << "推入就绪任务" << get_fd << std::endl;
        timer.pop(get_fd);
        epoller.delEvent(get_fd, 0);
        pool.submitWork(&print, get_fd);
      }
      // 从定时器链表中删除错误文件描述符，例如客户端主动关闭连接
      for (auto err_fd : err_fds) {
        std::cout << "删除主动关闭" << err_fd << std::endl;
        timer.pop(err_fd);
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
              for (auto timeout_fd : timeout_fds) {
                std::cout << "删除超时" << timeout_fd << std::endl;
                epoller.delEvent(timeout_fd, 1);
              }
              timeoutHandler();
              break;
            }
            case SIGTERM: { // 触发外部关闭程序的信号，关闭线程池
              std::cout << "触发终止信号" << std::endl;
              pool.shutdown();
              break;
            }
          }
      epoller.addEvent(signaler.pipe_fd_[0]);
      trigger_signal = false;
    }
  }

  return 0;
}