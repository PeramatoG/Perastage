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
  void Log(std::string msg);

private:
  Logger();
  ~Logger();
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  void Worker();

  // Flush policy: flush after every kFlushInterval messages or during shutdown.
  static constexpr std::size_t kFlushInterval = 32;

  std::ofstream file_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<std::string> queue_;
  bool done_ = false;
  std::thread worker_;
};
