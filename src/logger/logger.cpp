#include "logger.hpp"
#include "message_buffer.hpp"
#include <cassert>
#include <chrono>
#include <cstring>
#include <format>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

std::unique_ptr<Logger> Logger::ptr_ = nullptr;

std::fstream Logger::open_new_file(
    const std::chrono::time_point<std::chrono::system_clock> &tp,
    std::string_view name) {
  std::string full_file_name;
  const std::chrono::year_month_day ymd{
      std::chrono::floor<std::chrono::days>(tp)};
  auto p = name.find('/');
  assert(p == name.npos);
  full_file_name =
      std::format("{}_{:02d}_{:02d}_{}.log", static_cast<int>(ymd.year()),
                  static_cast<unsigned>(ymd.month()),
                  static_cast<unsigned>(ymd.day()), name);
  return std::fstream(logs_dir + full_file_name, std::ios_base::app);
}

void Logger::flush() {
  std::lock_guard lk(this->mutex_);
  fs_.flush();
}

Logger::Logger(const char *file_name, bool close_log, int log_buf_size,
               int split_lines, int max_queue_size)
    : close_(close_log), log_name_(file_name), split_lines_(split_lines),
      log_buffer_size_(log_buf_size), max_queue_size_(max_queue_size),
      queue_(max_queue_size_) {
  if (max_queue_size_ > 0) {
    std::thread t([this] { this->async_write_log(); });
    t.detach();
    is_async_ = true;
  }
  fs_ = open_new_file(std::chrono::system_clock::now(), file_name);
}
Logger::~Logger() {}
