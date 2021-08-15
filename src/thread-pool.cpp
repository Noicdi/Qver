/*
 * Author: xQmQ
 * Date: 2021/8/14 12:31
 * Description: 
 */
#include "../include/thread-pool.h"

void
ThreadPool::WorkThread_::operator()()
{
  std::function<void()> function; // 基础函数类

  bool fetch_queue;

  // 判断线程池是否关闭
  while (!pool_ptr_->shutdown_) {
    {
      // 线程工作环境加锁
      std::unique_lock<std::mutex> lock(pool_ptr_->conditional_mutex_);

      // 任务队列为空时，阻塞当前工作线程
      if (pool_ptr_->work_queue_.empty())
        pool_ptr_->conditional_lock.wait(lock);
      // 从任务队列取出任务并执行
      // 取出失败则进入下一次循环
      fetch_queue = pool_ptr_->work_queue_.fetchQueue(function);
    }
    if (fetch_queue)
      function();
  }
}

ThreadPool::ThreadPool(int thread_number)
    : thread_number_(thread_number), shutdown_(false), work_thread_queue_(std::vector<std::thread>(thread_number))
{
  for (int i = 0; i < thread_number_; ++i)
    work_thread_queue_[i] = std::thread(WorkThread_(i, this));
}

ThreadPool::~ThreadPool()
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

int
ThreadPool::size() const
{
  return thread_number_;
}