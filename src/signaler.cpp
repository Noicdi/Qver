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

// int
// Signaler::getPipefd(int count) const
// {
//   if (count == 0)
//     return pipe_fd_[0];
//   else if (count == 1)
//     return pipe_fd_[1];
//
//   return -1;
// }
//
// void
// Signaler::signalHandler(int signal)
// {
//   int save_errno = errno;
//   int msg = signal;
//   send(pipe_fd_[1], (char *)&msg, 1, 0);
//   errno = save_errno;
// }
//
// void
// Signaler::addSignal(int signal)
// {
//   struct sigaction sa;
//   memset(&sa, '\0', sizeof(sa));
//   sa.sa_handler = signalHandler;
//   sa.sa_flags |= SA_RESTART;
//   sigfillset(&sa.sa_mask);
//   assert(sigaction(signal, &sa, nullptr) != -1);
// }
//
// void
// Signaler::timeoutHandler()
// {
//   alarm(timeout_);
// }
