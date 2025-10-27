#ifndef TCP_SERVER_HPP_
#define TCP_SERVER_HPP_

#include "HTTPConn.hpp"
#include "epoller.hpp"
#include "heaptimer.hpp"
#include "thread_pool.hpp"
#include <bits/stdc++.h>

namespace Web {
class WebServer {
public:
  WebServer(int port, int trigMode, int timeoutMS, bool OptLinger,
            const char *dbName, int connPoolNum, int threadNum, bool closelog,
            int logQueSize);

  ~WebServer();
  void Start();

private:
  bool InitSocket_();
  void InitEventMode_(int trigMode);
  void AddClient_(int fd, sockaddr_in addr);

  void DealListen_();
  void DealWrite_(HTTPConn *client);
  void DealRead_(HTTPConn *client);

  void SendError_(int fd, const char *info);
  void ExtentTime_(HTTPConn *client);
  void CloseConn_(HTTPConn *client);

  void OnRead_(HTTPConn *client);
  void OnWrite_(HTTPConn *client);
  void OnProcess(HTTPConn *client);

  static const int MAX_FD = 65536;

  static int SetFdNonblock(int fd);

  int port_;
  bool openLinger_;
  int timeoutMS_; /* 毫秒MS */
  bool isClose_;
  uint32_t listenFd_;
  char *srcDir_;

  uint32_t listenEvent_;
  uint32_t connEvent_;

  std::unique_ptr<HeapTimer> timer_;
  std::unique_ptr<ThreadPool> threadpool_;
  std::unique_ptr<Epoller> epoller_;
  std::unordered_map<int, HTTPConn> users_;
};

} // namespace Web
#endif