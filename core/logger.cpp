/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "logger.h"
#include <filesystem>
#include <iostream>
#include <vector>
#include <wx/stdpaths.h>

Logger &Logger::Instance() {
  static Logger instance;
  return instance;
}

Logger::Logger() {
  wxString dataDir = wxStandardPaths::Get().GetUserDataDir();
  std::string dataDirUtf8 = std::string(dataDir.ToUTF8());
  if (!dataDirUtf8.empty()) {
    std::filesystem::path logDir = std::filesystem::u8path(dataDirUtf8);
    std::error_code ec;
    std::filesystem::create_directories(logDir, ec);
    if (!ec) {
      std::filesystem::path logPath = logDir / "perastage.log";
      file_.open(logPath, std::ios::out | std::ios::trunc);
      if (!file_.is_open()) {
        std::cerr << "Warning: Unable to open log file at " << logPath.string()
                  << "; logging only to stderr." << std::endl;
      }
    } else {
      std::cerr << "Warning: Unable to create log directory " << logDir.string()
                << "; logging only to stderr." << std::endl;
    }
  } else {
    std::cerr << "Warning: Unable to resolve user data directory; logging only "
                 "to stderr."
              << std::endl;
  }
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

void Logger::Log(std::string msg) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(std::move(msg));
  }
  cv_.notify_one();
}

void Logger::Worker() {
  std::unique_lock<std::mutex> lock(mutex_);
  std::size_t messages_since_flush = 0;
  while (true) {
    cv_.wait(lock, [this] { return done_ || !queue_.empty(); });
    if (done_ && queue_.empty())
      break;
    const bool shutting_down = done_;
    std::vector<std::string> batch;
    batch.reserve(queue_.size());
    while (!queue_.empty()) {
      batch.emplace_back(std::move(queue_.front()));
      queue_.pop();
    }
    lock.unlock();
    std::string buffer;
    for (const auto &msg : batch) {
      buffer.append(msg);
      buffer.push_back('\n');
    }
    if (file_.is_open()) {
      file_ << buffer;
      messages_since_flush += batch.size();
      if (shutting_down || messages_since_flush >= kFlushInterval) {
        file_.flush();
        messages_since_flush = 0;
      }
    }
    std::cerr << buffer;
    lock.lock();
  }
}
