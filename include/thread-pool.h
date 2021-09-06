/*
 * Author: xQmQ
 * Date: 2021/8/14 6:45
 * Description: 线程池
 */

#ifndef QVER_INCLUDE_THREAD_POOL_H_
#define QVER_INCLUDE_THREAD_POOL_H_

#include "non-copyable.h"
#include "work-queue.h"

#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

class ThreadPool : NonCopyable {
public:
  /*
   * description: 构造函数，初始化工作线程队列
   * param: {thread_number->工作线程队列中线程数量}
   * return: {}
   */
  explicit ThreadPool(int thread_number = 5);

  ~ThreadPool() = default;

  /*
   * description: 线程池中工作线程个数
   * param: {}
   * return: {线程池中工作线程个数}
   */
  int size() const;

  /*
   * description: 工作线程的主要工作函数，采用后置返回类型
   * param: {f->任务函数; args->参数集}
   * return: {std::future<>->类型自动推导的工作线程结果}
   */
  template<typename F, typename... Args>
  auto submitWork(F &&f, Args &&...args) -> std::future<decltype(f(args...))>;

  /*
   * description: 将工作线程中的任务执行完，抛弃任务队列中的任务
   * param: {}
   * return: {}
   */
  void shutdown();

  /*
   * description: 查看线程池是否关闭
   * param: {}
   * return: {true->关闭; false->工作}
   */
  bool isShutdown() const;

private:
  int thread_number_;                           // 当前线程数量
  bool shutdown_;                               // 线程池是否关闭，true->关闭
  WorkQueue<std::function<void()>> work_queue_; // 存储读写请求的任务队列
  std::vector<std::thread> work_thread_queue_;  // 工作线程队列
  std::mutex conditional_mutex_;                // 线程休眠锁
  std::condition_variable conditional_lock;     // 线程环境锁
  class WorkThread_ {                           // 工作线程
  public:
    explicit WorkThread_(int id, ThreadPool *pool_ptr) : thread_id_(id), pool_ptr_(pool_ptr)
    {}

    /*
     * description: 重载 ()
     * param: {}
     * return: {}
     */
    void operator()();

  private:
    int thread_id_;        // 工作线程编号
    ThreadPool *pool_ptr_; // 所属线程池
  };
};

template<typename F, typename... Args>
auto
ThreadPool::submitWork(F &&f, Args &&...args) -> std::future<decltype(f(args...))>
{
  // 将传入的函数指针和参数集封装为 std::function
  std::function<decltype(f(args...))()> function = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

  //封装获取任务对象执行结果(std::future)，方便另外一个线程查看结果
  auto work_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(function);

  //利用正则表达式，返回一个函数对象
  std::function<void()> wrapper_function = [work_ptr]() {
    (*work_ptr)();
  };

  // 队列通用安全封包函数，并压入安全队列
  work_queue_.pushQueue(wrapper_function);

  // 唤醒一个等待中的线程
  conditional_lock.notify_one();

  // 返回先前注册的任务指针
  return work_ptr->get_future();
}

#endif //QVER_INCLUDE_THREAD_POOL_H_