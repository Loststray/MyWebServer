#ifndef EPOLLER_HPP_
#define EPOLLER_HPP_
#include <bits/stdc++.h>
#include <sys/epoll.h>

namespace Web {
class Epoller {
private:
  int fd_;
  std::vector<epoll_event> events_;
  static constexpr int MAX_EVENTS = 1024;

public:
  Epoller(int max_events = MAX_EVENTS);
  ~Epoller();
  bool AddFd(int fd,uint32_t events);
  bool ModFd(int fd,uint32_t events);
  bool DelFd(int fd);
  int Wait(int timeout);
};

} // namespace Web

#endif