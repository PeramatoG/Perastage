#include "logger.h"
#include <iostream>

Logger &Logger::Instance() {
  static Logger instance;
  return instance;
}

Logger::Logger() {
  file_.open("perastage.log", std::ios::out | std::ios::trunc);
  worker_ = std::thread(&Logger::Worker, this);
}

Logger::~Logger() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    done_ = true;
  }
  cv_.notify_one();
  if (worker_.joinable())
    worker_.join();
  if (file_.is_open())
    file_.close();
}

void Logger::Log(const std::string &msg) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(msg);
  }
  cv_.notify_one();
}

void Logger::Worker() {
  std::unique_lock<std::mutex> lock(mutex_);
  while (true) {
    cv_.wait(lock, [this] { return done_ || !queue_.empty(); });
    if (done_ && queue_.empty())
      break;
    auto msg = queue_.front();
    queue_.pop();
    lock.unlock();
    if (file_.is_open()) {
      file_ << msg << std::endl;
      file_.flush();
    }
    std::cerr << msg << std::endl;
    lock.lock();
  }
}
