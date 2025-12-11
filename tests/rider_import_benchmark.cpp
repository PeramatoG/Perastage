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
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <string>
#include <sstream>
#include <vector>
#include <wx/init.h>

#include "../core/riderimporter.h"
#include "../core/configmanager.h"
#include "../models/fixture.h"
#include "../models/truss.h"

namespace {
std::optional<std::size_t> ReadStatusFieldKb(const std::string &key) {
  std::ifstream status("/proc/self/status");
  std::string line;
  while (std::getline(status, line)) {
    if (line.rfind(key, 0) == 0) {
      std::istringstream ss(line.substr(key.size()));
      std::size_t value = 0;
      ss >> value;
      return value;
    }
  }
  return std::nullopt;
}

std::size_t ReadPeakRssKb() {
  auto value = ReadStatusFieldKb("VmHWM:\t");
  return value.value_or(0);
}

std::size_t ReadCurrentRssKb() {
  auto value = ReadStatusFieldKb("VmRSS:\t");
  return value.value_or(0);
}

struct IterationResult {
  double milliseconds = 0.0;
  std::size_t fixtures = 0;
  std::size_t trusses = 0;
  std::size_t peakDeltaKb = 0;
  std::size_t finalRssKb = 0;
};

IterationResult RunOnce(const std::string &path, std::size_t baselinePeak) {
  ConfigManager &cfg = ConfigManager::Get();
  cfg.Reset();

  RiderImporter importer;
  const auto start = std::chrono::steady_clock::now();
  const bool ok = importer.Import(path);
  const auto end = std::chrono::steady_clock::now();

  IterationResult result;
  result.milliseconds =
      std::chrono::duration<double, std::milli>(end - start).count();
  result.peakDeltaKb = ReadPeakRssKb() - baselinePeak;
  result.finalRssKb = ReadCurrentRssKb();

  if (ok) {
    const auto &scene = cfg.GetScene();
    result.fixtures = scene.fixtures.size();
    result.trusses = scene.trusses.size();
  }

  return result;
}
} // namespace

int main(int argc, char **argv) {
  wxInitializer initializer;
  if (!initializer.IsOk()) {
    std::cerr << "wxWidgets failed to initialize" << std::endl;
    return 1;
  }

  const std::string path =
      argc >= 2 ? argv[1] : std::string("tests/data/rider_large.txt");
  const int iterations = argc >= 3 ? std::max(1, std::atoi(argv[2])) : 1;

  const std::size_t baselinePeak = ReadPeakRssKb();
  std::vector<IterationResult> results;
  results.reserve(iterations);

  for (int i = 0; i < iterations; ++i) {
    results.push_back(RunOnce(path, baselinePeak));
  }

  const double averageMs =
      std::accumulate(results.begin(), results.end(), 0.0,
                      [](double acc, const IterationResult &r) {
                        return acc + r.milliseconds;
                      }) /
      results.size();
  const std::size_t maxPeakDelta =
      std::accumulate(results.begin(), results.end(), std::size_t{0},
                      [](std::size_t acc, const IterationResult &r) {
                        return std::max(acc, r.peakDeltaKb);
                      });
  const std::size_t lastRss = results.empty() ? 0 : results.back().finalRssKb;
  const std::size_t lastFixtures =
      results.empty() ? 0 : results.back().fixtures;
  const std::size_t lastTrusses = results.empty() ? 0 : results.back().trusses;

  std::cout << "Rider path: " << path << '\n'
            << "Iterations: " << iterations << '\n'
            << "Average import time (ms): " << averageMs << '\n'
            << "Peak RSS increase (kB): " << maxPeakDelta << '\n'
            << "Final RSS (kB): " << lastRss << '\n'
            << "Fixtures imported: " << lastFixtures << '\n'
            << "Trusses imported: " << lastTrusses << std::endl;

  std::cout << "\nPer-iteration details:" << std::endl;
  for (std::size_t i = 0; i < results.size(); ++i) {
    const auto &r = results[i];
    std::cout << "  Run " << (i + 1) << ": " << r.milliseconds
              << " ms, peak +" << r.peakDeltaKb << " kB, RSS "
              << r.finalRssKb << " kB, fixtures " << r.fixtures << "\n";
  }

  return 0;
}
