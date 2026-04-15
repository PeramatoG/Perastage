#pragma once

#include <string>
#include <vector>

#include <tinyxml2.h>

namespace peraviz::dmx {

std::string read_gdtf_description_xml(const std::string &gdtf_path);

std::vector<tinyxml2::XMLElement *> collect_elements_by_name(tinyxml2::XMLElement *root,
                                                             const std::string &name_lower);

std::vector<tinyxml2::XMLElement *> collect_direct_children_by_name(tinyxml2::XMLElement *root,
                                                                    const std::string &name_lower);

std::string read_attr_ci(tinyxml2::XMLElement *node,
                         const char *name_a,
                         const char *name_b);

std::string lower_ascii(std::string text);

std::string trim_ascii(std::string text);

} // namespace peraviz::dmx
