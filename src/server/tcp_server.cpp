#include "tcp_server.hpp"
#include "HTTPConn.hpp"
#include "config.hpp"
#include "epoller.hpp"
#include "heaptimer.hpp"
#include "logger.hpp"
#include "sqlite.hpp"
#include "thread_pool.hpp"
#include <cstdint>
#include <memory>

namespace Web {

WebServer::WebServer(int port, int trigMode, int timeoutMS, bool OptLinger,
                     const char *dbName, int connPoolNum, int threadNum,
                     bool closelog, int logQueSize)
    : port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS) {
  srcDir_ = getcwd(nullptr, 256);
  assert(srcDir_);
  strncat(srcDir_, "/resource/", 16);
  HTTPConn::userCount = 0;
  HTTPConn::srcDir = srcDir_;
  Database::SQLite::init(dbName, connPoolNum);
  threadpool_ = std::make_unique<ThreadPool>(threadNum);
  epoller_ = std::make_unique<Epoller>();
  timer_ = std::make_unique<HeapTimer>();
  InitEventMode_(trigMode);

  if (!InitSocket_()) {
    isClose_ = true;
  }

  if (!closelog) {
    Logger::init("log", closelog, 50000, logQueSize);
    if (isClose_) {
      LOG_ERROR("========== Server init error!==========");
    } else {
      LOG_INFO("========== Server init ==========");
      LOG_INFO("Port:{}, OpenLinger: {}", port_, OptLinger);
      LOG_INFO("Listen Mode: {}, OpenConn Mode: {}",
               (listenEvent_ & EPOLLET ? "ET" : "LT"),
               (connEvent_ & EPOLLET ? "ET" : "LT"));
      LOG_INFO("srcDir: {}", HTTPConn::srcDir);
      LOG_INFO("SqlConnPool num: {}, ThreadPool num: {}", connPoolNum,
               threadNum);
    }
  }
}

WebServer::~WebServer() {
  close(listenFd_);
  isClose_ = true;
  free(srcDir_);
}

void WebServer::InitEventMode_(int trigMode) {
  listenEvent_ = EPOLLRDHUP;
  connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
  switch (trigMode) {
  case 0:
    break;
  case 1:
    connEvent_ |= EPOLLET;
    break;
  case 2:
    listenEvent_ |= EPOLLET;
    break;
  case 3:
    listenEvent_ |= EPOLLET;
    connEvent_ |= EPOLLET;
    break;
  default:
    listenEvent_ |= EPOLLET;
    connEvent_ |= EPOLLET;
    break;
  }
  HTTPConn::mode = (connEvent_ & EPOLLET) ? TriggerMode::EdgeTrigger
                                          : TriggerMode::LevelTrigger;
}

void WebServer::Start() {
  int timeMS = -1; /* epoll wait timeout == -1 无事件将阻塞 */
  if (!isClose_) {
    LOG_INFO("========== Server start ==========");
  }
  while (!isClose_) {
    if (timeoutMS_ > 0) {
      timeMS = timer_->GetNextTick();
    }
    int eventCnt = epoller_->wait(timeMS);
    for (int i = 0; i < eventCnt; i++) {
      /* 处理事件 */
      auto &[events, data] = (*epoller_)[i];
      uint32_t fd = data.fd;
      if (fd == listenFd_) {
        DealListen_();
      } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
        assert(users_.count(fd) > 0);
        CloseConn_(&users_[fd]);
      } else if (events & EPOLLIN) {
        assert(users_.count(fd) > 0);
        DealRead_(&users_[fd]);
      } else if (events & EPOLLOUT) {
        assert(users_.count(fd) > 0);
        DealWrite_(&users_[fd]);
      } else {
        LOG_ERROR("Unexpected event");
      }
    }
  }
}

void WebServer::SendError_(int fd, const char *info) {
  assert(fd > 0);
  int ret = send(fd, info, strlen(info), 0);
  if (ret < 0) {
    LOG_WARN("send error to client[{}] error!", fd);
  }
  close(fd);
}

void WebServer::CloseConn_(HTTPConn *client) {
  assert(client);
  LOG_INFO("Client[{}] quit!", client->get_fd());
  epoller_->erase(client->get_fd());
  client->close();
}

void WebServer::AddClient_(int fd, sockaddr_in addr) {
  assert(fd > 0);
  users_[fd].init(fd, addr);
  if (timeoutMS_ > 0) {
    timer_->add(fd, timeoutMS_,
                std::bind(&WebServer::CloseConn_, this, &users_[fd]));
  }
  epoller_->insert(fd, EPOLLIN | connEvent_);
  SetFdNonblock(fd);
  LOG_INFO("Client[{}] in!", users_[fd].get_fd());
}

void WebServer::DealListen_() {
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  do {
    int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
    if (fd <= 0) {
      return;
    } else if (HTTPConn::userCount >= MAX_FD) {
      SendError_(fd, "Server busy!");
      LOG_WARN("Clients is full!");
      return;
    }
    AddClient_(fd, addr);
  } while (listenEvent_ & EPOLLET);
}

void WebServer::DealRead_(HTTPConn *client) {
  assert(client);
  ExtentTime_(client);
  threadpool_->enqueue(&WebServer::OnRead_, this, client);
}

void WebServer::DealWrite_(HTTPConn *client) {
  assert(client);
  ExtentTime_(client);
  threadpool_->enqueue(&WebServer::OnWrite_, this, client);
}

void WebServer::ExtentTime_(HTTPConn *client) {
  assert(client);
  if (timeoutMS_ > 0) {
    timer_->adjust(client->get_fd(), timeoutMS_);
  }
}

void WebServer::OnRead_(HTTPConn *client) {
  assert(client);
  int ret = -1;
  int readErrno = 0;
  ret = client->read(&readErrno);
  if (ret <= 0 && readErrno != EAGAIN) {
    CloseConn_(client);
    return;
  }
  OnProcess(client);
}

void WebServer::OnProcess(HTTPConn *client) {
  if (client->process()) {
    epoller_->update(client->get_fd(), connEvent_ | EPOLLOUT);
  } else {
    epoller_->update(client->get_fd(), connEvent_ | EPOLLIN);
  }
}

void WebServer::OnWrite_(HTTPConn *client) {
  assert(client);
  int ret = -1;
  int writeErrno = 0;
  ret = client->write(&writeErrno);
  if (client->to_write_bytes() == 0) {
    /* 传输完成 */
    if (client->is_keep_alive()) {
      OnProcess(client);
      return;
    }
  } else if (ret < 0) {
    if (writeErrno == EAGAIN) {
      /* 继续传输 */
      epoller_->update(client->get_fd(), connEvent_ | EPOLLOUT);
      return;
    }
  }
  CloseConn_(client);
}

/* Create listenFd */
bool WebServer::InitSocket_() {
  int ret;
  struct sockaddr_in addr;
  if (port_ > 65535 || port_ < 1024) {
    LOG_ERROR("Port:{} error!", port_);
    return false;
  }
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port_);
  struct linger optLinger = {0, 0};
  if (openLinger_) {
    /* 优雅关闭: 直到所剩数据发送完毕或超时 */
    optLinger.l_onoff = 1;
    optLinger.l_linger = 1;
  }

  listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listenFd_ < 0) {
    LOG_ERROR("Create socket error!");
    return false;
  }

  ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger,
                   sizeof(optLinger));
  if (ret < 0) {
    close(listenFd_);
    LOG_ERROR("Init linger error!");
    return false;
  }

  int optval = 1;
  /* 端口复用 */
  /* 只有最后一个套接字会正常接收数据。 */
  ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
                   sizeof(int));
  if (ret == -1) {
    LOG_ERROR("set socket setsockopt error !");
    close(listenFd_);
    return false;
  }

  ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
  if (ret < 0) {
    LOG_ERROR("Bind Port:%d error!", port_);
    close(listenFd_);
    return false;
  }

  ret = listen(listenFd_, 6);
  if (ret < 0) {
    LOG_ERROR("Listen port:%d error!", port_);
    close(listenFd_);
    return false;
  }
  ret = epoller_->insert(listenFd_, listenEvent_ | EPOLLIN);
  if (ret == 0) {
    LOG_ERROR("Add listen error!");
    close(listenFd_);
    return false;
  }
  SetFdNonblock(listenFd_);
  LOG_INFO("Server port:%d", port_);
  return true;
}

int WebServer::SetFdNonblock(int fd) {
  assert(fd > 0);
  return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

} // namespace Web