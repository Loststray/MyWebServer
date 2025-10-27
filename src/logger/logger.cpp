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
#include <thread>

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
  if (is_async_) {
    std::unique_lock<std::mutex> lk(flush_mtx_);
    cv_flush_.wait(
        lk, [this] { return pending_.load(std::memory_order_acquire) == 0; });
    std::lock_guard<std::mutex> io_lk(this->mutex_);
    fs_.flush();
  } else {
    std::lock_guard<std::mutex> io_lk(this->mutex_);
    fs_.flush();
  }
}

Logger::Logger(const char *file_name, bool close_log, int split_lines,
               int max_queue_size)
    : close_(close_log), log_name_(file_name), split_lines_(split_lines),
      max_queue_size_(max_queue_size), queue_(max_queue_size_) {
  if (max_queue_size_ > 0) {
    is_async_ = true;
    auto worker = std::thread([this] { this->async_write_log(); });
    worker.detach();
  }
  fs_ = open_new_file(std::chrono::system_clock::now(), file_name);
}
Logger::~Logger() { flush(); }
