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
#include "diagnostics/DiagnosticPaths.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <vector>

namespace {
// Converts a log level to a stable text label.
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

// Formats one log entry for persistent and console sinks.
std::string FormatLogLine(Logger::Level level, const std::string &msg) {
  std::ostringstream oss;
  oss << '[' << LevelToString(level) << "] " << msg;
  return oss.str();
}

} // namespace

// Returns the process-wide asynchronous logger instance.
Logger &Logger::Instance() {
  static Logger instance;
  return instance;
}

// Opens the persistent log file and starts the log worker.
Logger::Logger() {
  const std::filesystem::path logDir = diagnostics::DiagnosticPaths::LogsDirectory();
  if (!logDir.empty()) {
    std::string error;
    if (diagnostics::DiagnosticPaths::EnsureDirectory(logDir, &error)) {
      log_path_ = diagnostics::DiagnosticPaths::CurrentLogFile();
      std::error_code rotateEc;
      const std::filesystem::path previousLogPath =
          diagnostics::DiagnosticPaths::PreviousLogFile();
      if (std::filesystem::exists(log_path_, rotateEc)) {
        std::filesystem::remove(previousLogPath, rotateEc);
        rotateEc.clear();
        std::filesystem::rename(log_path_, previousLogPath, rotateEc);
      }
      file_.open(log_path_, std::ios::out | std::ios::trunc);
      if (!file_.is_open()) {
        std::cerr << "Warning: Unable to open log file at " << log_path_.string()
                  << "; logging only to stderr." << std::endl;
      }
    } else {
      std::cerr << "Warning: Unable to create log directory " << logDir.string()
                << ": " << error << "; logging only to stderr." << std::endl;
    }
  } else {
    std::cerr << "Warning: Unable to resolve diagnostics directory; logging only "
                 "to stderr."
              << std::endl;
  }
  worker_ = std::thread(&Logger::Worker, this);
}

// Flushes queued log entries and stops the log worker.
Logger::~Logger() { ShutdownForExit(); }

// Queues an informational log entry.
void Logger::Log(std::string msg) { Log(Level::Info, std::move(msg)); }

// Queues a log entry at the requested severity.
void Logger::Log(Level level, std::string msg) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!accepting_logs_ || done_ ||
        static_cast<int>(level) > static_cast<int>(min_level_))
      return;
    queue_.push({level, std::move(msg)});
  }
  cv_.notify_one();
}

// Updates the runtime minimum severity filter.
void Logger::SetMinLevel(Level level) {
  std::lock_guard<std::mutex> lock(mutex_);
  min_level_ = level;
}

// Returns the current runtime minimum severity filter.
Logger::Level Logger::GetMinLevel() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return min_level_;
}

// Blocks until the current queue has been written to the sinks.
void Logger::Flush() {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this] { return queue_.empty() && !writing_; });
  if (file_.is_open())
    file_.flush();
}

// Stops the worker quickly while preserving a bounded tail of shutdown logs.
void Logger::ShutdownForExit(std::string finalMessage) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (shutdown_started_)
      return;
    shutdown_started_ = true;
    accepting_logs_ = false;
    if (!finalMessage.empty() &&
        static_cast<int>(Level::Info) <= static_cast<int>(min_level_)) {
      queue_.push({Level::Info, std::move(finalMessage)});
    }
    while (queue_.size() > kShutdownDrainLimit)
      queue_.pop();
    done_ = true;
  }

  cv_.notify_all();
  if (worker_.joinable())
    worker_.join();
  if (file_.is_open()) {
    file_.flush();
    file_.close();
  }
}

// Returns recent formatted log lines for diagnostic reports.
std::vector<std::string> Logger::GetRecentLines(std::size_t maxLines) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (maxLines >= recent_lines_.size())
    return recent_lines_;
  return std::vector<std::string>(recent_lines_.end() - maxLines,
                                  recent_lines_.end());
}

// Returns the persistent log file path used by the logger.
std::filesystem::path Logger::GetLogFilePath() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return log_path_;
}

// Drains the log queue to the persistent file and stderr.
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
      for (const auto &entry : batch) {
        std::string formatted = FormatLogLine(entry.level, entry.msg);
        recent_lines_.push_back(formatted);
        if (recent_lines_.size() > kMaxRecentLines) {
          recent_lines_.erase(
              recent_lines_.begin(),
              recent_lines_.begin() +
                  static_cast<std::ptrdiff_t>(recent_lines_.size() - kMaxRecentLines));
        }
        writing_ = true;
        lock.unlock();
        if (file_.is_open()) {
          file_ << formatted << '\n';
          ++messages_since_flush;
          if (messages_since_flush >= kFlushInterval) {
            file_.flush();
            messages_since_flush = 0;
          }
        }
        std::cerr << formatted << '\n';
        lock.lock();
        writing_ = false;
        cv_.notify_all();
        if (done_ && shutdown_started_)
          break;
      }
      cv_.notify_all();
    }
    if (file_.is_open() && shutting_down && messages_since_flush > 0) {
      file_.flush();
      messages_since_flush = 0;
    }
  }
}
