#ifndef SOCKET_HPP_
#define SOCKET_HPP_

#include <bits/stdc++.h>
#include <sys/socket.h>

namespace Web {

class Mysocket {
private:
  int fd_;

public:
  Mysocket(int domain, int type);
  ~Mysocket();
  operator bool() const { return fd_ != -1; }

};

} // namespace Web

#endif