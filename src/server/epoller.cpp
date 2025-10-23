#include "epoller.hpp"
#include <cstdint>
#include <sys/epoll.h>
#include <unistd.h>
namespace Web {
Epoller::Epoller(int max_events)
    : fd_(epoll_create1(EPOLL_CLOEXEC)), events_(max_events) {
  // insert check here...
}

Epoller::~Epoller() { close(fd_); }

bool Epoller::AddFd(int fd, uint32_t events) {
  if (fd < 0) {
    return false;
  }
  epoll_event ev = {};
  ev.data.fd = fd;
  ev.events = events;
  return epoll_ctl(fd_, EPOLL_CTL_ADD, fd, &ev) == 0;
}
bool Epoller::ModFd(int fd, uint32_t events) {
  if (fd < 0) {
    return false;
  }
  epoll_event ev = {};
  ev.data.fd = fd;
  ev.events = events;
  return epoll_ctl(fd_, EPOLL_CTL_MOD, fd, &ev) == 0;
}
bool Epoller::DelFd(int fd) {
  if (fd < 0) {
    return false;
  }
  epoll_event ev = {};
  return epoll_ctl(fd_, EPOLL_CTL_ADD, fd, &ev) == 0;
}



} // namespace Web