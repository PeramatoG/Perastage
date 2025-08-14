#pragma once
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

// Simple asynchronous logger that writes messages to stderr and a log file.
class Logger {
public:
  // Access singleton instance, creating log file on first use.
  static Logger &Instance();

  // Queue a message to be logged.
  void Log(const std::string &msg);

private:
  Logger();
  ~Logger();
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  void Worker();

  std::ofstream file_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<std::string> queue_;
  bool done_ = false;
  std::thread worker_;
};
