/*
 * Author: xQmQ
 * Date: 2021/8/14 6:45
 * Description: 线程池中用来保存读写请求的任务队列，供工作线程使用
 */

#ifndef QVER_INCLUDE_WORK_QUEUE_H_
#define QVER_INCLUDE_WORK_QUEUE_H_

#include "../include/non-copyable.h"

#include <mutex>
#include <queue>

template<typename T>
class WorkQueue : NonCopyable {
public:
  explicit WorkQueue() = default;

  ~WorkQueue() = default;

  bool empty();

  int size();

  /*
   * description: 向队尾推入元素
   * param: {t->被推入元素}
   * return: {}
   */
  void pushQueue(const T &t);

  /*
   * description: 从队首摘出元素
   * param: {t->用以保存被摘出元素}
   * return: {true->摘出元素成功; false->队列为空}
   */
  bool popQueue(T &t);

private:
  std::queue<T> work_queue_; // 任务队列
  std::mutex mutex_;         // 互斥锁
};

template<typename T>
bool
WorkQueue<T>::empty()
{
  std::unique_lock<std::mutex> lock(mutex_);

  return work_queue_.empty();
}

template<typename T>
int
WorkQueue<T>::size()
{
  std::unique_lock<std::mutex> lock(mutex_);

  return work_queue_.size();
}

template<typename T>
void
WorkQueue<T>::pushQueue(const T &t)
{
  std::unique_lock<std::mutex> lock(mutex_);
  work_queue_.template emplace(t);
}

template<typename T>
bool
WorkQueue<T>::popQueue(T &t)
{
  std::unique_lock<std::mutex> lock(mutex_);
  if (work_queue_.empty()) return false;

  t = std::move(work_queue_.front());
  work_queue_.pop();

  return true;
}

#endif //QVER_INCLUDE_WORK_QUEUE_H_
