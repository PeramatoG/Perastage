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

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace layouts {
struct LayoutDefinition;
}

namespace gui::layoutperf {

struct LayoutElementCounts {
  size_t projectLayouts = 0;
  size_t view2d = 0;
  size_t legends = 0;
  size_t eventTables = 0;
  size_t textBlocks = 0;
  size_t images = 0;
};

class LayoutRenderProfiler {
public:
  explicit LayoutRenderProfiler(std::string operationName);

  void SetLayoutContext(size_t projectLayoutCount,
                        const layouts::LayoutDefinition &layout);
  void SetLayoutContext(LayoutElementCounts counts, std::string activeLayoutName);
  void BeginPhase(std::string phaseName);
  void EndPhase();
  void RecordRenderedElement();
  void RecordReusedElement();
  void Finish(const std::string &result);

private:
  struct PhaseTiming {
    std::string name;
    long long durationMs = 0;
  };

  bool enabled_ = false;
  std::string operationName_;
  LayoutElementCounts counts_;
  std::string activeLayoutName_;
  size_t renderedElements_ = 0;
  size_t reusedElements_ = 0;
  std::chrono::steady_clock::time_point operationStart_;
  std::chrono::steady_clock::time_point phaseStart_;
  std::string currentPhase_;
  std::vector<PhaseTiming> phases_;
};

} // namespace gui::layoutperf
