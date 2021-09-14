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

#define MAX_EVENT_NUMBER 1024

class Epoller : NonCopyable {
public:
  Epoller();

  ~Epoller();

  /*
   * description: 向 epoll 内核事件表中注册事件
   * param: {fd->待注册的文件描述符; IN->监控读事件; OUT->监控写事件; ONESHOT->单词触发}
   * return: {0->注册成功; -1->注册失败}
   */
  int addEvent(int fd, bool IN, bool OUT, bool ONESHOT) const;

  /*
   * description: 向 epoll 内核事件表中修改事件
   * param: {fd->待修改的文件描述符; mod_event->预备修改的事件集}
   * return: {0->修改成功; -1->修改失败}
   */
  int modEvent(int fd, int mod_event) const;

  /*
   * description: 向 epoll 内核事件表中删除事件，并决定是否 close()
   * param: {fd->待删除的文件描述符; CLOSE->删除后是否 close()}
   * return: {0->删除成功; -1->删除失败}
   */
  int delEvent(int fd, bool CLOSE) const;

  /*
   * description: 获取就绪的文件描述符
   * param: {fds->记录就绪的文件描述符集}
   * return: {-1->获取失败; 0->没有就绪文件描述符; >0->共获取的文件描述符数}
   */
  int getFds(epoll_event *events);

private:
  int epoll_fd_; // epoll 文件描述符
};

#endif //QVER_INCLUDE_EPOLLER_H_
