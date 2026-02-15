#include "gdtf_scene_builder.h"

#include "asset_cache.h"
#include "coordinate_mapper.h"
#include "matrixutils.h"

#include <algorithm>
#include <cmath>
#include <array>
#include <cctype>
#include <cstring>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <tinyxml2.h>

namespace {

using peraviz::SceneNode;
std::string lower_ascii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

bool looks_like_axis(const std::string &tag_name, const std::string &name) {
    const std::string tag = lower_ascii(tag_name);
    const std::string n = lower_ascii(name);
    return tag.find("axis") != std::string::npos || n.find("pan") != std::string::npos ||
           n.find("tilt") != std::string::npos || n.find("yoke") != std::string::npos ||
           n.find("head") != std::string::npos;
}

bool looks_like_emitter(const std::string &tag_name, const std::string &name) {
    const std::string tag = lower_ascii(tag_name);
    const std::string n = lower_ascii(name);
    return tag.find("beam") != std::string::npos || tag.find("laser") != std::string::npos ||
           n.find("lens") != std::string::npos || n.find("emitter") != std::string::npos;
}




bool parse_gdtf_4x4_matrix(const char *text, Matrix &out) {
    if (!text) {
        return false;
    }

    std::string cleaned;
    cleaned.reserve(std::strlen(text));
    for (const char c : std::string(text)) {
        if (c == '{' || c == '}' || c == ',') {
            cleaned.push_back(' ');
        } else {
            cleaned.push_back(c);
        }
    }

    std::stringstream stream(cleaned);
    std::vector<float> values;
    float value = 0.0F;
    while (stream >> value) {
        values.push_back(value);
    }

    if (values.size() != 16) {
        return false;
    }

    // Perastage currently interprets matrix strings in MatrixUtils::ParseMatrix
    // using MVR-oriented indexing. For GDTF fixture geometry in Peraviz we need
    // the canonical 4x4 layout where rows represent U, V, W and translation.
    out.u = std::array<float, 3>{values[0], values[1], values[2]};
    out.v = std::array<float, 3>{values[4], values[5], values[6]};
    out.w = std::array<float, 3>{values[8], values[9], values[10]};
    out.o = std::array<float, 3>{values[12], values[13], values[14]};
    return true;
}

bool is_supported_geometry_tag(const std::string &tag_name) {
    return tag_name == "Geometry" || tag_name == "Axis" || tag_name == "Beam" ||
           tag_name == "GeometryReference" || tag_name == "Laser" ||
           tag_name == "WiringObject" || tag_name == "Inventory" ||
           tag_name == "Structure" || tag_name == "Support" ||
           tag_name == "Magnet" || tag_name == "Display" ||
           tag_name == "MediaServerLayer" || tag_name == "MediaServerCamera" ||
           tag_name == "MediaServerMaster" || tag_name.rfind("Filter", 0) == 0;
}

Matrix parse_local_matrix(tinyxml2::XMLElement *node) {
    Matrix out = MatrixUtils::Identity();

    const char *position = node->Attribute("Position");
    if (!position) {
        position = node->Attribute("position");
    }
    if (position) {
        if (!parse_gdtf_4x4_matrix(position, out)) {
            MatrixUtils::ParseMatrix(position, out);
        }
        return out;
    }

    const char *matrix_attr = node->Attribute("Matrix");
    if (!matrix_attr) {
        matrix_attr = node->Attribute("matrix");
    }
    if (matrix_attr) {
        if (!parse_gdtf_4x4_matrix(matrix_attr, out)) {
            MatrixUtils::ParseMatrix(matrix_attr, out);
        }
        return out;
    }

    tinyxml2::XMLElement *matrix = node->FirstChildElement("Matrix");
    if (!matrix) {
        matrix = node->FirstChildElement("matrix");
    }
    if (matrix) {
        if (const char *text = matrix->GetText()) {
            if (!parse_gdtf_4x4_matrix(text, out)) {
                MatrixUtils::ParseMatrix(text, out);
            }
        }
    }
    return out;
}

std::string safe_name(tinyxml2::XMLElement *node, const std::string &fallback) {
    if (const char *name = node->Attribute("Name")) {
        return name;
    }
    if (const char *name = node->Attribute("name")) {
        return name;
    }
    return fallback;
}

} // namespace

namespace peraviz {

std::vector<SceneNode> build_fixture_geometry_nodes(const GdtfBuildRequest &request,
                                                    const std::string &parent_id,
                                                    const Matrix &parent_world,
                                                    int &extracted_asset_count) {
    std::vector<SceneNode> nodes;

    ZipAssetCache gdtf_cache(request.gdtf_archive_path);
    const std::string description_path = gdtf_cache.ensure_extracted("description.xml");
    if (description_path.empty()) {
        return nodes;
    }

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(description_path.c_str()) != tinyxml2::XML_SUCCESS) {
        return nodes;
    }

    tinyxml2::XMLElement *fixture_type = doc.FirstChildElement("GDTF");
    if (fixture_type) {
        fixture_type = fixture_type->FirstChildElement("FixtureType");
    }
    if (!fixture_type) {
        fixture_type = doc.FirstChildElement("FixtureType");
    }
    if (!fixture_type) {
        return nodes;
    }

    std::unordered_map<std::string, std::string> model_file_by_name;
    if (tinyxml2::XMLElement *models = fixture_type->FirstChildElement("Models")) {
        for (tinyxml2::XMLElement *model = models->FirstChildElement(); model;
             model = model->NextSiblingElement()) {
            const char *name = model->Attribute("Name");
            if (!name) {
                name = model->Attribute("name");
            }
            const char *file = model->Attribute("File");
            if (!file) {
                file = model->Attribute("file");
            }
            if (!name || !file) {
                continue;
            }
            model_file_by_name[name] = file;
        }
    }

    std::string root_geometry_name;
    if (tinyxml2::XMLElement *dmx_modes = fixture_type->FirstChildElement("DMXModes")) {
        for (tinyxml2::XMLElement *mode = dmx_modes->FirstChildElement("DMXMode"); mode;
             mode = mode->NextSiblingElement("DMXMode")) {
            const char *mode_name = mode->Attribute("Name");
            if (!mode_name) {
                mode_name = mode->Attribute("name");
            }
            if (!request.gdtf_mode.empty() && mode_name && request.gdtf_mode != mode_name) {
                continue;
            }
            const char *geometry = mode->Attribute("Geometry");
            if (!geometry) {
                geometry = mode->Attribute("geometry");
            }
            if (geometry) {
                root_geometry_name = geometry;
                break;
            }
        }
    }

    tinyxml2::XMLElement *geometries = fixture_type->FirstChildElement("Geometries");
    if (!geometries) {
        return nodes;
    }

    std::unordered_map<std::string, tinyxml2::XMLElement *> geometry_by_name;
    for (tinyxml2::XMLElement *geometry = geometries->FirstChildElement(); geometry;
         geometry = geometry->NextSiblingElement()) {
        const std::string geometry_name = safe_name(geometry, "geometry");
        if (!geometry_name.empty()) {
            geometry_by_name[geometry_name] = geometry;
        }
    }

    tinyxml2::XMLElement *root_geometry = nullptr;
    for (tinyxml2::XMLElement *geometry = geometries->FirstChildElement(); geometry;
         geometry = geometry->NextSiblingElement()) {
        if (root_geometry_name.empty()) {
            root_geometry = geometry;
            break;
        }
        const std::string geometry_name = safe_name(geometry, "geometry");
        if (geometry_name == root_geometry_name) {
            root_geometry = geometry;
            break;
        }
    }

    if (!root_geometry) {
        return nodes;
    }

    int local_counter = 0;
    std::function<void(tinyxml2::XMLElement *, const std::string &, const Matrix &, const char *,
                       const Matrix *)>
        append_geometry;
    append_geometry = [&](tinyxml2::XMLElement *geometry, const std::string &geometry_parent_id,
                          const Matrix &geometry_parent_world, const char *override_model,
                          const Matrix *prepend_local) {
        const std::string geometry_tag = geometry->Name() ? geometry->Name() : "";
        Matrix local = parse_local_matrix(geometry);
        if (prepend_local) {
            local = MatrixUtils::Multiply(*prepend_local, local);
        }
        Matrix world = MatrixUtils::Multiply(geometry_parent_world, local);

        if (geometry_tag == "GeometryReference") {
            const char *referenced_geometry_name = geometry->Attribute("Geometry");
            if (!referenced_geometry_name) {
                referenced_geometry_name = geometry->Attribute("geometry");
            }
            if (!referenced_geometry_name) {
                return;
            }

            auto referenced_it = geometry_by_name.find(referenced_geometry_name);
            if (referenced_it == geometry_by_name.end() || !referenced_it->second) {
                return;
            }

            const char *reference_model = geometry->Attribute("Model");
            if (!reference_model) {
                reference_model = geometry->Attribute("model");
            }
            append_geometry(referenced_it->second, geometry_parent_id, geometry_parent_world,
                            reference_model ? reference_model : override_model, &local);
            return;
        }

        const std::string geometry_name = safe_name(geometry, "geometry");
        const std::string geometry_id = request.fixture_node_id + "/" + geometry_name +
                                        "#" + std::to_string(local_counter++);

        SceneNode node;
        node.node_id = geometry_id;
        node.parent_id = geometry_parent_id;
        node.name = geometry_name;
        node.type = "fixture_geometry";
        node.is_fixture = true;
        node.is_axis = looks_like_axis(geometry->Name(), geometry_name);
        node.is_emitter = looks_like_emitter(geometry->Name(), geometry_name);
        node.local_transform = peraviz::coordinate_mapper::to_godot_transform(local);

        const char *model_name = override_model ? override_model : geometry->Attribute("Model");
        if (!model_name) {
            model_name = geometry->Attribute("model");
        }
        if (model_name) {
            auto model_it = model_file_by_name.find(model_name);
            if (model_it != model_file_by_name.end()) {
                node.asset_path = gdtf_cache.ensure_gdtf_model_extracted(model_it->second);
            }
        }

        nodes.push_back(node);

        for (tinyxml2::XMLElement *child = geometry->FirstChildElement(); child;
             child = child->NextSiblingElement()) {
            const std::string child_tag = child->Name() ? child->Name() : "";
            if (!is_supported_geometry_tag(child_tag)) {
                continue;
            }
            append_geometry(child, geometry_id, world, nullptr, nullptr);
        }
    };

    append_geometry(root_geometry, parent_id, parent_world, nullptr, nullptr);
    extracted_asset_count += gdtf_cache.extracted_assets();
    return nodes;
}

} // namespace peraviz
