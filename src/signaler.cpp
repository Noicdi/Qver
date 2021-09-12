/*
 * Author: xQmQ
 * Date: 2021/8/27 16:07
 * Description: 
 */

#include "../include/signaler.h"

Signaler::Signaler(int timeout) : timeout_(timeout)
{
  int result = socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, pipe_fd_);
  assert(result != -1);
}

Signaler::~Signaler()
{
  close(pipe_fd_[0]);
  close(pipe_fd_[1]);
}
