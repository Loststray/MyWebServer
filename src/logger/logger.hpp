#ifndef LOGGER_HPP_
#define LOGGER_HPP_

#include "message_buffer.hpp"
#include <chrono>
#include <format>
#include <fstream>
#include <string_view>
#include <utility>
#include <condition_variable>
#include <atomic>
#include <thread>

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
  void write_log(int level, std::format_string<Args...> format, Args &&...args) {
    auto current_timestamp = std::chrono::system_clock().now();
    std::string res = std::format("[{0:%T}] ", current_timestamp) +
                      std::format("{:7}", Levels[level]) +
                      std::format(format, std::forward<Args>(args)...);
    {
      std::lock_guard lk(this->mutex_);
      count_++;
      if (count_ % split_lines_ == 0) {
        fs_.flush();
        fs_.close();
        std::string newname = this->log_name_;
        newname += std::format("_{}", count_ / split_lines_);
        fs_ = open_new_file(current_timestamp, newname);
      }
      if (!is_async_) {
        fs_ << res << '\n';
        return;
      }
    }
    // Async path: increase pending and enqueue
    pending_.fetch_add(1, std::memory_order_relaxed);
    queue_.push_back(std::move(res));
  }

  void flush(void);

  Logger(const Logger&) = delete;
  Logger(Logger&&) = delete;
  Logger& operator=(const Logger&) = delete;
  Logger& operator=(Logger&&) = delete;

  // Public destructor so std::unique_ptr can delete the singleton at exit
  ~Logger();

private:
  Logger(const char *file_name, bool close_log, int log_buf_size,
         int split_lines, int max_queue_size);
  std::fstream
  open_new_file(const std::chrono::time_point<std::chrono::system_clock> &tp,
                std::string_view name);
  void async_write_log() {
    std::string log;
    while (queue_.pop_front(log)) {
      {
        std::lock_guard<std::mutex> lk(mutex_);
        fs_ << log << '\n';
      }
      auto left = pending_.fetch_sub(1, std::memory_order_acq_rel) - 1;
      if (left == 0) {
        std::lock_guard<std::mutex> lk(flush_mtx_);
        cv_flush_.notify_all();
      }
    }
  }

private:
  static std::unique_ptr<Logger> ptr_;
  [[maybe_unused]] bool close_;
  std::string log_name_;             // log文件名
  int split_lines_;                  // 日志最大行数
  [[maybe_unused]] int log_buffer_size_;              // 日志缓冲区大小
  int max_queue_size_;               // 队列大小
  std::string dir_name_;             // 路径名
  size_t count_;                     // 日志行数记录
  MessageBuffer<std::string> queue_; // 队列
  bool is_async_;                    // 是否同步标志位
  std::mutex mutex_;
  std::fstream fs_;
  // For flush synchronization in async mode
  std::condition_variable cv_flush_;
  std::mutex flush_mtx_;
  std::atomic<size_t> pending_{0};
};

template <class... Arg> void LOG_DEBUG(std::format_string<Arg...> format, Arg &&...args) {
  Logger::get_instance()->write_log(0, format, std::forward<Arg>(args)...);
}
template <class... Arg> void LOG_INFO(std::format_string<Arg...> format, Arg &&...args) {
  Logger::get_instance()->write_log(1, format, std::forward<Arg>(args)...);
}
template <class... Arg> void LOG_WARN(std::format_string<Arg...> format, Arg &&...args) {
  Logger::get_instance()->write_log(2, format, std::forward<Arg>(args)...);
}
template <class... Arg> void LOG_ERROR(std::format_string<Arg...> format, Arg &&...args) {
  Logger::get_instance()->write_log(3, format, std::forward<Arg>(args)...);
}
template <class... Arg> void LOG_FATAL(std::format_string<Arg...> format, Arg &&...args) {
  Logger::get_instance()->write_log(4, format, std::forward<Arg>(args)...);
}

#endif
