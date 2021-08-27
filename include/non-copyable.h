/*
 * Author: xQmQ
 * Date: 2021/8/22 16:57
 * Description: 禁止拷贝构造和拷贝操作符
 */

#ifndef QVER_INCLUDE_NON_COPYABLE_H_
#define QVER_INCLUDE_NON_COPYABLE_H_

class NonCopyable {
public:
  NonCopyable(const NonCopyable &) = delete;

  NonCopyable(NonCopyable &&) = delete;

  NonCopyable &operator=(const NonCopyable &) = delete;

  NonCopyable &operator=(NonCopyable &&) = delete;

protected:
  NonCopyable() = default;

  ~NonCopyable() = default;
};

#endif //QVER_INCLUDE_NON_COPYABLE_H_
