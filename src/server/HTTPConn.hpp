#ifndef HTTP_CONNECTION_HPP_
#define HTTP_CONNECTION_HPP_

#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "buffer.hpp"
#include "config.hpp"

#include <arpa/inet.h>
#include <atomic>

namespace Web {

class HTTPConn {
public:
  HTTPConn();

  ~HTTPConn();

  void init(int sockFd, const sockaddr_in &addr);

  ssize_t read(int *saveErrno);

  ssize_t write(int *saveErrno);

  void close();

  int get_fd() const;

  int get_port() const;

  const char *get_IP() const;

  sockaddr_in get_addr() const;

  bool process();

  int to_write_bytes() { return iov_[0].iov_len + iov_[1].iov_len; }

  bool is_keep_alive() const { return request_.IsKeepAlive(); }

  static TriggerMode mode;
  static const char *srcDir;
  static std::atomic<int> userCount;

private:
  int fd_;
  struct sockaddr_in addr_;

  bool close_;

  int iov_cnt_;
  struct iovec iov_[2];

  Buffer readBuff_;  // 读缓冲区
  Buffer writeBuff_; // 写缓冲区

  HTTPRequest request_;
  HTTPResponse response_;
};

} // namespace Web

#endif