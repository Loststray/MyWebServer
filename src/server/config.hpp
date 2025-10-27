#ifndef CONFIG_HPP_
#define CONFIG_HPP_
namespace Web {

enum class TriggerMode { EdgeTrigger = 0, LevelTrigger = 1 };

class Config {

public:
  Config();
  void parse_arg(int argc, char *argv[]);

  // 端口号
  int PORT;

  // 日志写入方式
  int LOGWrite;

  // 触发组合模式
  int TRIGMode;

  // listenfd触发模式
  TriggerMode LISTENTrigmode;

  // connfd触发模式
  TriggerMode CONNTrigmode;

  // 优雅关闭链接
  int OPT_LINGER;

  // 数据库连接池数量
  int sql_num;

  // 线程池内的线程数量
  int thread_num;

  // 是否关闭日志
  bool close_log;
};
} // namespace Web

#endif