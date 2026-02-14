#include "gdtf_scene_builder.h"

#include "asset_cache.h"
#include "matrixutils.h"

#include <algorithm>
#include <cmath>
#include <array>
#include <cctype>
#include <functional>
#include <unordered_map>

#include <tinyxml2.h>

namespace {

using peraviz::SceneNode;
using peraviz::Vec3;

Vec3 map_position(const std::array<float, 3> &source_mm) {
    return Vec3{source_mm[0] / 1000.0F, source_mm[2] / 1000.0F,
                -source_mm[1] / 1000.0F};
}

std::array<float, 3> map_axis(const std::array<float, 3> &v) {
    return {v[0], v[2], -v[1]};
}

Matrix to_godot_basis_matrix(const Matrix &source) {
    Matrix out;
    out.u = map_axis(source.u);
    out.v = map_axis(source.v);
    out.w = map_axis(source.w);
    out.o = {0.0F, 0.0F, 0.0F};
    return out;
}

std::array<float, 3> extract_scale(const Matrix &m) {
    auto len = [](const std::array<float, 3> &v) {
        return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    };
    return {len(m.u), len(m.v), len(m.w)};
}

Matrix normalize_basis(const Matrix &m, const std::array<float, 3> &scale) {
    Matrix out = m;
    auto safe_div = [](float value, float s) {
        return (std::abs(s) > 1e-6F) ? value / s : value;
    };
    for (int i = 0; i < 3; ++i) {
        out.u[i] = safe_div(out.u[i], scale[0]);
        out.v[i] = safe_div(out.v[i], scale[1]);
        out.w[i] = safe_div(out.w[i], scale[2]);
    }
    return out;
}

peraviz::SceneTransform to_godot_transform(const Matrix &local_transform) {
    peraviz::SceneTransform transform;
    transform.position = map_position(local_transform.o);

    Matrix basis = to_godot_basis_matrix(local_transform);
    const auto scale = extract_scale(basis);
    transform.scale = {scale[0], scale[1], scale[2]};

    Matrix rotation_only = normalize_basis(basis, scale);
    const auto euler = MatrixUtils::MatrixToEuler(rotation_only);
    transform.rotation_degrees = {euler[0], euler[1], euler[2]};
    return transform;
}

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

Matrix parse_local_matrix(tinyxml2::XMLElement *node) {
    Matrix out = MatrixUtils::Identity();
    if (tinyxml2::XMLElement *matrix = node->FirstChildElement("Matrix")) {
        if (const char *text = matrix->GetText()) {
            MatrixUtils::ParseMatrix(text, out);
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
            const char *file = model->Attribute("File");
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
            if (!request.gdtf_mode.empty() && mode_name && request.gdtf_mode != mode_name) {
                continue;
            }
            if (const char *geometry = mode->Attribute("Geometry")) {
                root_geometry_name = geometry;
                break;
            }
        }
    }

    tinyxml2::XMLElement *geometries = fixture_type->FirstChildElement("Geometries");
    if (!geometries) {
        return nodes;
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
    std::function<void(tinyxml2::XMLElement *, const std::string &, const Matrix &)> append_geometry;
    append_geometry = [&](tinyxml2::XMLElement *geometry, const std::string &geometry_parent_id,
                          const Matrix &geometry_parent_world) {
        const std::string geometry_name = safe_name(geometry, "geometry");
        const std::string geometry_id = request.fixture_node_id + "/" + geometry_name +
                                        "#" + std::to_string(local_counter++);

        Matrix local = parse_local_matrix(geometry);
        Matrix world = MatrixUtils::Multiply(geometry_parent_world, local);

        SceneNode node;
        node.node_id = geometry_id;
        node.parent_id = geometry_parent_id;
        node.name = geometry_name;
        node.type = "fixture_geometry";
        node.is_fixture = true;
        node.is_axis = looks_like_axis(geometry->Name(), geometry_name);
        node.is_emitter = looks_like_emitter(geometry->Name(), geometry_name);
        node.local_transform = to_godot_transform(local);

        if (const char *model_name = geometry->Attribute("Model")) {
            auto model_it = model_file_by_name.find(model_name);
            if (model_it != model_file_by_name.end()) {
                node.asset_path = gdtf_cache.ensure_extracted(model_it->second);
            }
        }

        nodes.push_back(node);

        for (tinyxml2::XMLElement *child = geometry->FirstChildElement(); child;
             child = child->NextSiblingElement()) {
            if (!child->Attribute("Name") && !child->Attribute("name")) {
                continue;
            }
            append_geometry(child, geometry_id, world);
        }
    };

    append_geometry(root_geometry, parent_id, parent_world);
    extracted_asset_count += gdtf_cache.extracted_assets();
    return nodes;
}

} // namespace peraviz
