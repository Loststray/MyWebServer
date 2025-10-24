// Basic logger test using CTest
#include "logger.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main() {
  namespace fs = std::filesystem;

  // Ensure working log directory exists (logger writes to "log/")
  std::error_code ec;
  fs::create_directories("log", ec);

  // Initialize logger singleton
  bool created =
      Logger::init("unit_test", /*close_log=*/false, 8192, 100, 1024);
  if (!created && Logger::get_instance() == nullptr) {
    std::cerr << "Logger init failed" << std::endl;
    return 1;
  }

  // Write a couple of log lines
  LOG_INFO("Hello {}", 123);
  LOG_DEBUG("debug {}", 114514);
  LOG_ERROR("Oops {}", "err");
  Logger::get_instance()->flush();

  // Compute today's log filename to read back
  auto now = std::chrono::system_clock::now();
  const std::chrono::year_month_day ymd{
      std::chrono::floor<std::chrono::days>(now)};
  std::string path =
      std::format("log/{}_{:02d}_{:02d}_{}.log", static_cast<int>(ymd.year()),
                  static_cast<unsigned>(ymd.month()),
                  static_cast<unsigned>(ymd.day()), "unit_test");

  std::ifstream in(path);
  if (!in.is_open()) {
    std::cerr << "Failed to open log file: " << path << std::endl;
    return 1;
  }

  std::vector<std::string> lines;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty())
      lines.push_back(line);
  }
  in.close();

  if (lines.empty()) {
    std::cerr << "No log lines found in file" << std::endl;
    return 1;
  }

  bool has_info = false, has_error = false;
  for (const auto &l : lines) {
    if (l.find("[INFO]") != std::string::npos)
      has_info = true;
    if (l.find("[ERROR]") != std::string::npos)
      has_error = true;
  }

  if (!has_info || !has_error) {
    std::cerr << "Expected both INFO and ERROR entries" << std::endl;
    return 1;
  }

  return 0;
}
