/*
 * Author: xQmQ
 * Date: 2021/8/24 19:37
 * Description: 
 */

#include "../include/timer.h"

Timer::~Timer()
{
  UtilTimer *temp_ptr = head_ptr_;
  while (temp_ptr) {
    head_ptr_ = temp_ptr->next_ptr_;
    delete temp_ptr;
    temp_ptr = head_ptr_;
  }
}

void
Timer::push(int fd)
{
  if (fd < 0)
    return;

  UtilTimer *temp_ptr = new UtilTimer(fd);

  if (!head_ptr_) {
    head_ptr_ = tail_ptr_ = temp_ptr;
    return;
  }

  temp_ptr->prev_ptr_ = tail_ptr_;
  tail_ptr_->next_ptr_ = temp_ptr;
  tail_ptr_ = temp_ptr;
}

int
Timer::pop(int fd)
{
  if (fd < 0 || !head_ptr_)
    return -1;

  UtilTimer *temp_ptr = head_ptr_;
  while (temp_ptr) {
    if (temp_ptr->fd_ == fd)
      break;
    temp_ptr = temp_ptr->next_ptr_;
  }

  if (temp_ptr == head_ptr_ && temp_ptr == tail_ptr_) {
    delete temp_ptr;
    head_ptr_ = tail_ptr_ = nullptr;
  } else if (temp_ptr == head_ptr_) {
    head_ptr_ = head_ptr_->next_ptr_;
    head_ptr_->prev_ptr_ = nullptr;
    delete temp_ptr;
  } else if (temp_ptr == tail_ptr_) {
    tail_ptr_ = tail_ptr_->prev_ptr_;
    tail_ptr_->next_ptr_ = nullptr;
    delete temp_ptr;
  } else {
    temp_ptr->prev_ptr_->next_ptr_ = temp_ptr->next_ptr_;
    temp_ptr->next_ptr_->prev_ptr_ = temp_ptr->prev_ptr_;
    delete temp_ptr;
  }
  return 0;
}

void
Timer::tick(std::vector<int> &timeout_fds)
{
  timeout_fds.clear();

  if (head_ptr_) {
    time_t cur_time = time(nullptr);
    UtilTimer *temp_ptr = head_ptr_;
    while (temp_ptr) {
      if (cur_time < temp_ptr->expiration_time_)
        break;
      timeout_fds.push_back(temp_ptr->fd_);
      pop(temp_ptr->fd_);
      temp_ptr = head_ptr_;
    }
  }
}
