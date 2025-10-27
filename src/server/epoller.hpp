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
  bool insert(int fd, uint32_t events);
  bool update(int fd, uint32_t events);
  bool erase(int fd);
  int wait(int timeout);
  epoll_event &operator[](const size_t &x) { return events_[x]; }
};

} // namespace Web

#endif