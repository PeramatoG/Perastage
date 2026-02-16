#pragma once

#include "scene_model.h"
#include "types.h"

#include <string>
#include <vector>

namespace peraviz {

struct GdtfBuildRequest {
    std::string gdtf_archive_path;
    std::string gdtf_mode;
    std::string fixture_node_id;
    std::string fixture_name;
};

std::vector<SceneNode> build_fixture_geometry_nodes(const GdtfBuildRequest &request,
                                                    const std::string &parent_id,
                                                    const Matrix &parent_world,
                                                    int &extracted_asset_count);

} // namespace peraviz
