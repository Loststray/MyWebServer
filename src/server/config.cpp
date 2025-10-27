#include "config.hpp"
#include <cstdlib>
#include <unistd.h>
namespace Web {

Config::Config() {
  // 端口号
  PORT = 1453;

  // 日志写入方式，默认同步
  LOGWrite = 0;

  // 触发组合模式,默认listenfd LT + connfd LT
  TRIGMode = 0;

  // listenfd触发模式，默认LT
  LISTENTrigmode = TriggerMode::LevelTrigger;

  // connfd触发模式，默认LT
  CONNTrigmode = TriggerMode::LevelTrigger;

  // 优雅关闭链接，默认不使用
  OPT_LINGER = 0;

  // 数据库连接池数量,默认8
  sql_num = 8;

  // 线程池内的线程数量,默认8
  thread_num = 8;

  // 关闭日志,默认不关闭
  close_log = false;
}

void Config::parse_arg(int argc, char *argv[]) {
  int opt;
  const char *str = "p:l:m:o:s:t:c:a:";
  while ((opt = getopt(argc, argv, str)) != -1) {
    switch (opt) {
    case 'p': {
      PORT = atoi(optarg);
      break;
    }
    case 'l': {
      LOGWrite = atoi(optarg);
      break;
    }
    case 'm': {
      TRIGMode = atoi(optarg);
      if (TRIGMode & 1) {
        CONNTrigmode = TriggerMode::EdgeTrigger;
      }
      if (TRIGMode & 2) {
        LISTENTrigmode = TriggerMode::EdgeTrigger;
      }
      break;
    }
    case 'o': {
      OPT_LINGER = atoi(optarg);
      break;
    }
    case 's': {
      sql_num = atoi(optarg);
      break;
    }
    case 't': {
      thread_num = atoi(optarg);
      break;
    }
    case 'c': {
      close_log = atoi(optarg);
      break;
    }
    default:
      break;
    }
  }
}
} // namespace Web