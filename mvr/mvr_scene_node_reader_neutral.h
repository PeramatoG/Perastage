#pragma once

#include "mvr_scene_node_reader.h"

#include <string>

namespace tinyxml2 {
class XMLElement;
}

namespace mvr::scene_reader_detail {

MvrNeutralSceneNode BuildNeutralSceneNode(tinyxml2::XMLElement *element,
                                          const std::string &kind,
                                          const std::string &layerUuid,
                                          const std::string &layerName,
                                          const std::string &parentGroupUuid);

} // namespace mvr::scene_reader_detail
