/*
 * Author: xQmQ
 * Date: 2021/8/24 19:37
 * Description: 处理非活动连接的定时器，对超时的非活动连接做关闭操作
 */

#ifndef QVER_INCLUDE_TIMER_H_
#define QVER_INCLUDE_TIMER_H_

#include "non-copyable.h"

#include <time.h>
#include <vector>

class Timer : NonCopyable {
public:
  explicit Timer() : head_ptr_(nullptr), tail_ptr_(nullptr)
  {}

  ~Timer();

  /*
   * description: 向定时容器中添加待监控的 socket
   * param: {fd->待监控的 socket}
   * return: {}
   */
  void push(int fd);

  /*
   * description: 删除特定的 socket
   * param: {fd->被删除的fd}
   * return: {-1->删除失败; 0->删除成功}
   */
  int pop(int fd);

  /*
   * description: 收集到期文件描述符集，由其他方法做处理
   * param: {timeout_fds->到期文件描述符集}
   * return: {}
   */
  void tick(std::vector<int> &timeout_fds);

private:
  class UtilTimer {
  public:
    explicit UtilTimer(int fd, int timeout = 10) : fd_(fd),
                                                   expiration_time_(time(nullptr) + timeout),
                                                   prev_ptr_(nullptr),
                                                   next_ptr_(nullptr)
    {}

  public:
    int fd_;                 // 被监控的文件描述符
    time_t expiration_time_; // 超时时间，绝对时间
    UtilTimer *prev_ptr_;    // 前一个计时器
    UtilTimer *next_ptr_;    // 后一个计时器
  };

  UtilTimer *head_ptr_; // 头结点
  UtilTimer *tail_ptr_; // 尾结点
};

#endif //QVER_INCLUDE_TIMER_H_
