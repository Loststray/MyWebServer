#ifndef LOGGER_HPP_
#define LOGGER_HPP_

#include "message_buffer.hpp"
#include <chrono>
#include <format>
#include <fstream>
#include <string_view>
#include <utility>

namespace {
const char *Levels[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]", "[FATAL]"};
const char *logs_dir = "log/";
} // namespace
class Logger {
public:
  static Logger *get_instance() {
    return ptr_.get();
  }
  // 可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
  static bool init(const char *file_name, bool close_log,
                   int log_buf_size = 8192, int split_lines = 5000000,
                   int max_queue_size = 0) {
    if (!ptr_) {
      ptr_ = std::unique_ptr<Logger>(new Logger(
          file_name, close_log, log_buf_size, split_lines, max_queue_size));
      return true;
    }
    return false;
  }
  template <class... Args>
  void write_log(int level, std::string_view format, Args &&...args) {
    auto current_timestamp = std::chrono::system_clock().now();
    // level > 0 && level < 5
    std::lock_guard lk(this->mutex_);
    count_++;
    if (count_ % split_lines_ == 0) {
      fs_.flush();
      fs_.close();
      std::string newname = this->log_name_;
      newname += std::format("_{}", count_ / split_lines_);
      fs_ = open_new_file(current_timestamp, newname);
    }
    std::string res;
    res = std::format("[{0:%T}] ", current_timestamp) +
          std::format("{:7}", Levels[level]) +
          std::format(format, std::forward(args)...);
    if (is_async_) {
      queue_.push_back(std::move(res));
    } else {
      fs_ << res << '\n';
    }
  }

  void flush(void);

  Logger(const Logger&) = delete;
  Logger(Logger&&) = delete;
  Logger& operator=(const Logger&) = delete;
  Logger& operator=(Logger&&) = delete;

private:
  Logger(const char *file_name, bool close_log, int log_buf_size,
         int split_lines, int max_queue_size);
  ~Logger();
  std::fstream
  open_new_file(const std::chrono::time_point<std::chrono::system_clock> &tp,
                std::string_view name);
  void async_write_log() {
    std::string single_log;
    // 从阻塞队列中取出一个日志string，写入文件
    while (!queue_.empty()) {
      std::lock_guard<std::mutex> lk(mutex_);
      auto log = queue_.front();
      queue_.pop_front();
      fs_ << log << '\n';
    }
  }

private:
  static std::unique_ptr<Logger> ptr_;
  bool close_;
  std::string log_name_;             // log文件名
  int split_lines_;                  // 日志最大行数
  int log_buffer_size_;              // 日志缓冲区大小
  int max_queue_size_;               // 队列大小
  std::string dir_name_;             // 路径名
  size_t count_;                     // 日志行数记录
  MessageBuffer<std::string> queue_; // 队列
  bool is_async_;                    // 是否同步标志位
  std::mutex mutex_;
  std::fstream fs_;
};

template <class... Arg> void LOG_DEBUG(std::string_view format, Arg &&...args) {
  Logger::get_instance()->write_log(0, format, std::forward(args)...);
}
template <class... Arg> void LOG_INFO(std::string_view format, Arg &&...args) {
  Logger::get_instance()->write_log(1, format, std::forward(args)...);
}
template <class... Arg> void LOG_WARN(std::string_view format, Arg &&...args) {
  Logger::get_instance()->write_log(2, format, std::forward(args)...);
}
template <class... Arg> void LOG_ERROR(std::string_view format, Arg &&...args) {
  Logger::get_instance()->write_log(3, format, std::forward(args)...);
}
template <class... Arg> void LOG_FATAL(std::string_view format, Arg &&...args) {
  Logger::get_instance()->write_log(4, format, std::forward(args)...);
}

#endif
