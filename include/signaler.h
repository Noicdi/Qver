/*
 * Author: xQmQ
 * Date: 2021/8/27 16:07
 * Description: 信号处理
 */

#ifndef QVER_INCLUDE_SIGNALER_H_
#define QVER_INCLUDE_SIGNALER_H_

#include "non-copyable.h"
#include "timer.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

class Signaler : NonCopyable {
public:
  Signaler(int timeout = 5);

  ~Signaler();

  /*
   * description: 返回 pipe_fd_
   * param: {count->pipe_fd_ 的编号}
   * return: {pipe_fd_[0]; pipe_fd_[1]; -1->返回失败}
   */
  // int getPipefd(int count) const;

  /*
   * description: 信号处理函数，用于通知主循环有信号触发
   * param: {signal->被触发的信号}
   * return: {}
   */
  // void signalHandler(int signal);

  /*
   * description: 添加需要处理的信号
   * param: {signal->需要处理的信号}
   * return: {}
   */
  // void addSignal(int signal);

  /*
   * description: 设置定时器超时时间
   * param: {}
   * return: {}
   */
  // void timeoutHandler();

public:
  int pipe_fd_[2]; // 用于通知主循环有信号触发
  int timeout_;    // 定时器信号触发时间
};

#endif //QVER_INCLUDE_SIGNALER_H_
