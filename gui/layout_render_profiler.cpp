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
#include "layout_render_profiler.h"

#include <sstream>
#include <utility>

#include "LayoutCollection.h"
#include "logger.h"

namespace gui::layoutperf {
namespace {

// Reports whether debug-only layout performance traces should be emitted.
bool IsLayoutPerformanceLoggingEnabled() {
#ifndef NDEBUG
  return true;
#else
  return false;
#endif
}

// Converts a steady-clock duration to whole milliseconds for compact logs.
long long ToMilliseconds(std::chrono::steady_clock::duration duration) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(duration)
      .count();
}

} // namespace

// Creates a profiler for one layout loading or rendering operation.
LayoutRenderProfiler::LayoutRenderProfiler(std::string operationName)
    : enabled_(IsLayoutPerformanceLoggingEnabled()),
      operationName_(std::move(operationName)),
      operationStart_(std::chrono::steady_clock::now()) {}

// Stores the active layout and project-wide layout count for later logging.
void LayoutRenderProfiler::SetLayoutContext(
    size_t projectLayoutCount, const layouts::LayoutDefinition &layout) {
  LayoutElementCounts counts;
  counts.projectLayouts = projectLayoutCount;
  counts.view2d = layout.view2dViews.size();
  counts.legends = layout.legendViews.size();
  counts.eventTables = layout.eventTables.size();
  counts.textBlocks = layout.textViews.size();
  counts.images = layout.imageViews.size();
  SetLayoutContext(counts, layout.name);
}

// Stores precomputed layout element counts for later logging.
void LayoutRenderProfiler::SetLayoutContext(LayoutElementCounts counts,
                                            std::string activeLayoutName) {
  if (!enabled_)
    return;
  counts_ = counts;
  activeLayoutName_ = std::move(activeLayoutName);
}

// Starts timing a named phase, closing any previous phase first.
void LayoutRenderProfiler::BeginPhase(std::string phaseName) {
  if (!enabled_)
    return;
  EndPhase();
  currentPhase_ = std::move(phaseName);
  phaseStart_ = std::chrono::steady_clock::now();
}

// Stops timing the current phase and stores its duration.
void LayoutRenderProfiler::EndPhase() {
  if (!enabled_ || currentPhase_.empty())
    return;
  phases_.push_back({currentPhase_, ToMilliseconds(
                                        std::chrono::steady_clock::now() -
                                        phaseStart_)});
  currentPhase_.clear();
}

// Counts a layout element whose texture or capture was rebuilt in this operation.
void LayoutRenderProfiler::RecordRenderedElement() {
  if (!enabled_)
    return;
  ++renderedElements_;
}

// Counts a layout element whose existing cache was reused in this operation.
void LayoutRenderProfiler::RecordReusedElement() {
  if (!enabled_)
    return;
  ++reusedElements_;
}

// Emits one compact debug log line with counts and phase timings.
void LayoutRenderProfiler::Finish(const std::string &result) {
  if (!enabled_)
    return;
  EndPhase();
  std::ostringstream phaseStream;
  for (size_t i = 0; i < phases_.size(); ++i) {
    if (i > 0)
      phaseStream << ',';
    phaseStream << phases_[i].name << '=' << phases_[i].durationMs << "ms";
  }

  std::ostringstream message;
  message << "Layout performance: operation=" << operationName_
          << " result=" << result
          << " project_layouts=" << counts_.projectLayouts
          << " active_layout=\"" << activeLayoutName_ << '"'
          << " views=" << counts_.view2d << " legends=" << counts_.legends
          << " event_tables=" << counts_.eventTables
          << " text_blocks=" << counts_.textBlocks
          << " images=" << counts_.images
          << " rendered_elements=" << renderedElements_
          << " reused_cached_elements=" << reusedElements_
          << " total_ms="
          << ToMilliseconds(std::chrono::steady_clock::now() - operationStart_)
          << " phases=[" << phaseStream.str() << ']';
  Logger::Instance().Log(Logger::Level::Debug, message.str());
}

} // namespace gui::layoutperf
