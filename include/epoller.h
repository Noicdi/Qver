/*
 * Author: xQmQ
 * Date: 2021/8/22 16:00
 * Description: 注册事件并监听
 */

#ifndef QVER_INCLUDE_EPOLLER_H_
#define QVER_INCLUDE_EPOLLER_H_

#include "non-copyable.h"

#include <assert.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>

#define MAX_EVENT_NUMBER 1024

class Epoller : NonCopyable {
public:
  Epoller();

  ~Epoller() = default;

  /*
   * description: 向 epoll 内核事件表中注册事件
   * param: {fd->被注册的文件描述符}
   * return: {0->注册成功; -1->注册失败}
   */
  int addEvent(int fd) const;

  /*
   * description: 向 epoll 内核事件表中删除事件，并决定是否 close()
   * param: {fd->被注册的文件描述符; is_close->1表示关闭,0表示不关闭}
   * return: {0->删除成功; -1->删除失败}
   */
  int delEvent(int fd, int is_close) const;

  /*
   * description: 获取就绪的文件描述符
   * param: {fds->记录就绪的文件描述符集}
   * return: {-1->获取失败; 0->没有就绪文件描述符; >0->共获取的文件描述符数}
   */
  int getFds(std::vector<int> &get_fds, std::vector<int> &err_fds);

private:
  int epoll_fd_; // epoll 文件描述符
  struct epoll_event events_[MAX_EVENT_NUMBER] {
  }; // epoll 检测到的事件集
};

#endif //QVER_INCLUDE_EPOLLER_H_
