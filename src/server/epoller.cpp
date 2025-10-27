#include "epoller.hpp"
#include <cstdint>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
namespace Web {
Epoller::Epoller(int max_events)
    : fd_(epoll_create1(EPOLL_CLOEXEC)), events_(max_events) {
  // insert check here...
}

Epoller::~Epoller() { close(fd_); }

bool Epoller::insert(int fd, uint32_t events) {
  if (fd < 0) {
    return false;
  }
  epoll_event ev = {};
  ev.data.fd = fd;
  ev.events = events;
  return epoll_ctl(fd_, EPOLL_CTL_ADD, fd, &ev) == 0;
}
bool Epoller::update(int fd, uint32_t events) {
  if (fd < 0) {
    return false;
  }
  epoll_event ev = {};
  ev.data.fd = fd;
  ev.events = events;
  return epoll_ctl(fd_, EPOLL_CTL_MOD, fd, &ev) == 0;
}
bool Epoller::erase(int fd) {
  if (fd < 0) {
    return false;
  }
  return epoll_ctl(fd_, EPOLL_CTL_ADD, fd, NULL) == 0;
}
int Epoller::wait(int timeout) {
  return epoll_wait(fd_, &events_[0], events_.size(), timeout);
}

} // namespace Web