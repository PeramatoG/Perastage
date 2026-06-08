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
#pragma once
#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <queue>
#include <vector>
#include <string>
#include <thread>

// Simple asynchronous logger that writes messages to stderr and a log file.
class Logger {
public:
  enum class Level {
    Error = 0,
    Warn = 1,
    Info = 2,
    Debug = 3,
  };

  // Access singleton instance, creating log file on first use.
  static Logger &Instance();

  // Queue a message to be logged.
  void Log(std::string msg);
  void Log(Level level, std::string msg);

  // Runtime minimum level filter. Messages below this level are discarded.
  void SetMinLevel(Level level);
  Level GetMinLevel() const;

  // Flushes queued messages to the persistent sinks.
  void Flush();

  // Stops background logging during application exit after a bounded final drain.
  void ShutdownForExit(std::string finalMessage = {});

  // Returns a copy of the newest formatted log lines kept in memory.
  std::vector<std::string> GetRecentLines(std::size_t maxLines) const;

  // Returns the current persistent log file path.
  std::filesystem::path GetLogFilePath() const;

private:
  Logger();
  ~Logger();
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  void Worker();

  // Flush policy: flush after every kFlushInterval messages or during shutdown.
  static constexpr std::size_t kFlushInterval = 32;
  // Limit batch sizes to avoid large memory spikes when the queue grows.
  static constexpr std::size_t kMaxBatchSize = 256;
  static constexpr std::size_t kMaxRecentLines = 512;
  static constexpr std::size_t kShutdownDrainLimit = 32;

  std::filesystem::path log_path_;
  std::ofstream file_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  struct Entry {
    Level level = Level::Info;
    std::string msg;
  };
  std::queue<Entry> queue_;
  std::vector<std::string> recent_lines_;
  // Default to most-verbose so level filtering is opt-in and does not
  // accidentally suppress Debug logs unless explicitly configured.
  Level min_level_ = Level::Debug;
  bool accepting_logs_ = true;
  bool shutdown_started_ = false;
  bool done_ = false;
  bool writing_ = false;
  std::thread worker_;
};
