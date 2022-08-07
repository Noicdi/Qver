/*
 * Author: xQmQ
 * Date: 2021/8/14 12:31
 * Description: 线程池的具体实现
 */
#include "../include/thread-pool.h"

void
ThreadPool::WorkThread_::operator()()
{
  std::function<void()> function; // 基础函数类

  bool pop_queue;

  // 判断线程池是否关闭
  while (!pool_ptr_->shutdown_) {
    {
      // 线程工作环境加锁
      std::unique_lock<std::mutex> lock(pool_ptr_->conditional_mutex_);

      // 任务队列为空时，阻塞当前工作线程
      pool_ptr_->conditional_lock.wait(
          lock,
          [this] { return !pool_ptr_->work_queue_.empty(); });
      // 从任务队列取出任务并执行
      // 取出失败则进入下一次循环
      pop_queue = pool_ptr_->work_queue_.popQueue(function);
    }
    if (pop_queue)
      function();
  }
}

/*
 * 在构造函数中泄露 this 指针到其他线程中
 * 这是否符合线程安全的规范？
 * 后期考虑是否需要重写
 * 应当创建一个 create()
 * 用来注册线程到工作线程队列中
 * ---
 * 但是考虑到这个服务器会先初始化
 * 再插入任务
 * 暂时这样设定
 */
ThreadPool::ThreadPool(const int thread_number)
    : thread_number_(thread_number), shutdown_(false), work_thread_queue_(std::vector<std::thread>(thread_number))
{
  for (int i = 0; i < thread_number_; ++i)
    work_thread_queue_[i] = std::thread(WorkThread_(i, this));
}

/*
 * 当外部信号强迫程序结束且任务未结束
 * 会调用析构函数
 * 析构函数会唤醒工作线程
 * 执行未完成的任务
 * 工作线程需要调用 ThreadPool::conditional_mutex_
 * 来确保互斥环境
 * 但是析构函数可能会销毁 ThreadPool::conditional_mutex_
 * 破坏工作线程的互斥环境
 * 应当创建一个 shutdown()
 * 用来结束所有工作线程的任务
 */
// ThreadPool::~ThreadPool()
// {
//   // 关闭线程池
//   shutdown_ = true;
//
//   // 唤醒所有工作线程
//   // 工作线程根据 shutdown_ 关闭工作循环
//   conditional_lock.notify_all();
//
//   for (auto &work_thread_ : work_thread_queue_)
//     if (work_thread_.joinable())
//       work_thread_.join();
// }

int
ThreadPool::size() const
{
  return thread_number_;
}

void
ThreadPool::shutdown()
{
  // 关闭线程池
  shutdown_ = true;

  // 唤醒所有工作线程
  // 工作线程根据 shutdown_ 关闭工作循环
  conditional_lock.notify_all();

  for (auto &work_thread_ : work_thread_queue_)
    if (work_thread_.joinable())
      work_thread_.join();
}

bool
ThreadPool::isShutdown() const
{
  return shutdown_;
}
