#include "HTTPConn.hpp"
#include "config.hpp"
#include "logger.hpp"
using namespace Web;

const char *HTTPConn::srcDir;
std::atomic<int> HTTPConn::userCount;
TriggerMode HTTPConn::mode = TriggerMode::LevelTrigger;

HTTPConn::HTTPConn() {
  fd_ = -1;
  addr_ = {};
  close_ = true;
};

HTTPConn::~HTTPConn() { close(); };

void HTTPConn::init(int fd, const sockaddr_in &addr) {
  assert(fd > 0);
  userCount++;
  addr_ = addr;
  fd_ = fd;
  writeBuff_.RetrieveAll();
  readBuff_.RetrieveAll();
  close_ = false;
  LOG_INFO("Client[{}]({}:{}) in, userCount:{}", fd_, get_IP(), get_port(),
           userCount.load());
}

void HTTPConn::close() {
  response_.UnmapFile();
  if (close_ == false) {
    close_ = true;
    userCount--;
    ::close(fd_);
    LOG_INFO("Client[{}]({}:{}) quit, UserCount:{}", fd_, get_IP(), get_port(),
             userCount.load());
  }
}

int HTTPConn::get_fd() const { return fd_; };

struct sockaddr_in HTTPConn::get_addr() const { return addr_; }

const char *HTTPConn::get_IP() const { return inet_ntoa(addr_.sin_addr); }

int HTTPConn::get_port() const { return addr_.sin_port; }

ssize_t HTTPConn::read(int *saveErrno) {
  ssize_t len = -1;
  do {
    len = readBuff_.ReadFd(fd_, saveErrno);
    if (len <= 0) {
      break;
    }
  } while (mode == TriggerMode::EdgeTrigger);
  return len;
}

ssize_t HTTPConn::write(int *saveErrno) {
  ssize_t len = -1;
  do {
    len = writev(fd_, iov_, iov_cnt_);
    if (len <= 0) {
      *saveErrno = errno;
      break;
    }
    if (iov_[0].iov_len + iov_[1].iov_len == 0) {
      break;
    } /* 传输结束 */
    else if (static_cast<size_t>(len) > iov_[0].iov_len) {
      iov_[1].iov_base = (uint8_t *)iov_[1].iov_base + (len - iov_[0].iov_len);
      iov_[1].iov_len -= (len - iov_[0].iov_len);
      if (iov_[0].iov_len) {
        writeBuff_.RetrieveAll();
        iov_[0].iov_len = 0;
      }
    } else {
      iov_[0].iov_base = (uint8_t *)iov_[0].iov_base + len;
      iov_[0].iov_len -= len;
      writeBuff_.Retrieve(len);
    }
  } while (mode == TriggerMode::EdgeTrigger || to_write_bytes() > 10240);
  return len;
}

bool HTTPConn::process() {
  request_.init();
  if (readBuff_.ReadableBytes() <= 0) {
    return false;
  } else if (request_.parse(readBuff_)) {
    LOG_DEBUG("%s", request_.path().c_str());
    response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
  } else {
    response_.Init(srcDir, request_.path(), false, 400);
  }

  response_.MakeResponse(writeBuff_);
  /* 响应头 */
  iov_[0].iov_base = const_cast<char *>(writeBuff_.Peek());
  iov_[0].iov_len = writeBuff_.ReadableBytes();
  iov_cnt_ = 1;

  /* 文件 */
  if (response_.FileLen() > 0 && response_.File()) {
    iov_[1].iov_base = response_.File();
    iov_[1].iov_len = response_.FileLen();
    iov_cnt_ = 2;
  }
  LOG_DEBUG("filesize:{}, {} to {}", response_.FileLen(), iov_cnt_,
            to_write_bytes());
  return true;
}
