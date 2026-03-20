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
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <vector>
#include <wx/stdpaths.h>

namespace {
const char *LevelToString(Logger::Level level) {
  switch (level) {
  case Logger::Level::Error:
    return "ERROR";
  case Logger::Level::Warn:
    return "WARN";
  case Logger::Level::Info:
    return "INFO";
  case Logger::Level::Debug:
    return "DEBUG";
  }
  return "INFO";
}

std::string FormatLogLine(Logger::Level level, const std::string &msg) {
  std::ostringstream oss;
  oss << '[' << LevelToString(level) << "] " << msg;
  return oss.str();
}
} // namespace

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

void Logger::Log(std::string msg) { Log(Level::Info, std::move(msg)); }

void Logger::Log(Level level, std::string msg) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (static_cast<int>(level) > static_cast<int>(min_level_))
      return;
    queue_.push({level, std::move(msg)});
  }
  cv_.notify_one();
}

void Logger::SetMinLevel(Level level) {
  std::lock_guard<std::mutex> lock(mutex_);
  min_level_ = level;
}

Logger::Level Logger::GetMinLevel() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return min_level_;
}

void Logger::Worker() {
  std::unique_lock<std::mutex> lock(mutex_);
  std::size_t messages_since_flush = 0;
  while (true) {
    cv_.wait(lock, [this] { return done_ || !queue_.empty(); });
    if (done_ && queue_.empty())
      break;
    const bool shutting_down = done_;
    while (!queue_.empty()) {
      const std::size_t batch_size =
          std::min(queue_.size(), kMaxBatchSize);
      std::vector<Entry> batch;
      batch.reserve(batch_size);
      for (std::size_t i = 0; i < batch_size; ++i) {
        batch.emplace_back(std::move(queue_.front()));
        queue_.pop();
      }
      lock.unlock();
      for (const auto &entry : batch) {
        std::string formatted = FormatLogLine(entry.level, entry.msg);
        if (file_.is_open()) {
          file_ << formatted << '\n';
          ++messages_since_flush;
          if (messages_since_flush >= kFlushInterval) {
            file_.flush();
            messages_since_flush = 0;
          }
        }
        std::cerr << formatted << '\n';
      }
      lock.lock();
    }
    if (file_.is_open() && shutting_down && messages_since_flush > 0) {
      file_.flush();
      messages_since_flush = 0;
    }
  }
}
