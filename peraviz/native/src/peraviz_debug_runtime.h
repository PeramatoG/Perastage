#pragma once

#include "matrixutils.h"
#include "scene_model.h"

#include <string>

namespace peraviz::debug_runtime {

void set_baseline_debug_enabled(bool enabled);
bool is_baseline_debug_enabled();

void set_coordinate_debug_enabled(bool enabled);
bool is_coordinate_debug_enabled();

void log_coordinate_mapping_metadata();

void log_coordinate_debug_event(const std::string &event,
                                const std::string &scope,
                                const std::string &detail);

void log_baseline_transform_comparison(const std::string &context,
                                       const Matrix &source_local,
                                       const SceneTransform &mapped_transform);

void log_transform_adjustment(const std::string &context,
                              const std::string &reason,
                              const Matrix &before,
                              const Matrix &after);

} // namespace peraviz::debug_runtime
