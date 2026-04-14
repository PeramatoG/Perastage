#include "gdtf_scene_builder.h"

#include "asset_cache.h"
#include "coordinate_mapper.h"
#include "matrixutils.h"
#include "peraviz_debug_runtime.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <functional>
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

bool looks_like_lens_geometry(const std::string &tag_name, const std::string &name,
                              bool parent_is_lens) {
    if (parent_is_lens) {
        return true;
    }
    const std::string tag = lower_ascii(tag_name);
    const std::string n = lower_ascii(name);
    return tag == "beam" || n.find("lens") != std::string::npos ||
           n.find("optic") != std::string::npos || n.find("glass") != std::string::npos;
}

bool looks_like_emitter(const std::string &tag_name, const std::string &name,
                        bool is_lens_geometry) {
    const std::string tag = lower_ascii(tag_name);
    const std::string n = lower_ascii(name);
    return tag.find("beam") != std::string::npos || tag.find("laser") != std::string::npos ||
           n.find("emitter") != std::string::npos || is_lens_geometry;
}

bool is_supported_geometry_tag(const std::string &tag_name) {
    const std::string lower = lower_ascii(tag_name);
    return lower == "geometry" || lower == "axis" || lower == "beam" ||
           lower == "geometryreference" || lower == "laser" ||
           lower == "wiringobject" || lower == "inventory" ||
           lower == "structure" || lower == "support" ||
           lower == "magnet" || lower == "display" ||
           lower == "mediaserverlayer" || lower == "mediaservercamera" ||
           lower == "mediaservermaster" || lower.rfind("filter", 0) == 0;
}

std::string infer_asset_kind_from_path(const std::string &asset_path) {
    if (asset_path.empty()) {
        return "none";
    }

    std::filesystem::path path = std::filesystem::u8path(asset_path);
    std::string extension = lower_ascii(path.extension().u8string());
    if (extension == ".3ds") {
        return "mesh";
    }
    if (extension == ".glb" || extension == ".gltf" || extension == ".obj" ||
        extension == ".dae" || extension == ".fbx" || extension == ".stl") {
        return "scene";
    }
    return "none";
}

void convert_translation_m_to_mm(Matrix &matrix, const std::string &context) {
    const Matrix before = matrix;
    matrix.o[0] *= 1000.0F;
    matrix.o[1] *= 1000.0F;
    matrix.o[2] *= 1000.0F;

    peraviz::debug_runtime::log_transform_adjustment(
        context,
        "GDTF local transforms store translation in meters while Peraviz matrix composition uses millimeters before Godot conversion.",
        before, matrix);
}

Matrix parse_local_matrix(tinyxml2::XMLElement *node) {
    Matrix out = MatrixUtils::Identity();

    const char *position = node->Attribute("Position");
    if (!position) {
        position = node->Attribute("position");
    }
    if (position) {
        MatrixUtils::ParseMatrix(position, out);
        // GDTF geometry transforms are authored in meters. The Peraviz scene
        // graph uses the same matrix utilities as MVR parsing, where
        // translations are represented in millimeters before conversion to
        // Godot meters.
        convert_translation_m_to_mm(out, "gdtf_scene_builder::parse_local_matrix(Position)");
        return out;
    }

    const char *matrix_attr = node->Attribute("Matrix");
    if (!matrix_attr) {
        matrix_attr = node->Attribute("matrix");
    }
    if (matrix_attr) {
        MatrixUtils::ParseMatrix(matrix_attr, out);
        convert_translation_m_to_mm(out, "gdtf_scene_builder::parse_local_matrix(MatrixAttribute)");
        return out;
    }

    tinyxml2::XMLElement *matrix = node->FirstChildElement("Matrix");
    if (!matrix) {
        matrix = node->FirstChildElement("matrix");
    }
    if (matrix) {
        if (const char *text = matrix->GetText()) {
            MatrixUtils::ParseMatrix(text, out);
            convert_translation_m_to_mm(out, "gdtf_scene_builder::parse_local_matrix(MatrixNode)");
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

bool parse_float_attr(tinyxml2::XMLElement *node, const char *name,
                      const char *alt_name, float &out_value) {
    const char *raw = node->Attribute(name);
    if (!raw && alt_name) {
        raw = node->Attribute(alt_name);
    }
    if (!raw) {
        return false;
    }

    char *end = nullptr;
    const float parsed = std::strtof(raw, &end);
    if (end == raw) {
        return false;
    }
    out_value = parsed;
    return true;
}

} // namespace

namespace peraviz {

std::vector<SceneNode> build_fixture_geometry_nodes(const GdtfBuildRequest &request,
                                                    const std::string &parent_id,
                                                    const Matrix &parent_world,
                                                    int &extracted_asset_count) {
    std::vector<SceneNode> nodes;

    ZipAssetCache gdtf_cache(request.gdtf_archive_path);
    const std::string description_path = gdtf_cache.ensure_archive_file_extracted("description.xml");
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

    struct GdtfModelVisual {
        std::string file;
        std::string primitive_type;
        float size_x = 0.1F;
        float size_y = 0.1F;
        float size_z = 0.1F;
    };
    std::unordered_map<std::string, GdtfModelVisual> model_visual_by_name;
    std::unordered_map<std::string, float> emitter_wavelength_by_name;
    if (tinyxml2::XMLElement *models = fixture_type->FirstChildElement("Models")) {
        for (tinyxml2::XMLElement *model = models->FirstChildElement(); model;
             model = model->NextSiblingElement()) {
            const char *name = model->Attribute("Name");
            if (!name) {
                name = model->Attribute("name");
            }
            if (!name) {
                continue;
            }

            GdtfModelVisual visual;
            const char *file = model->Attribute("File");
            if (!file) {
                file = model->Attribute("file");
            }
            if (file) {
                visual.file = file;
            }

            const char *primitive_type = model->Attribute("PrimitiveType");
            if (!primitive_type) {
                primitive_type = model->Attribute("primitivetype");
            }
            if (primitive_type) {
                visual.primitive_type = primitive_type;
            }

            parse_float_attr(model, "Length", "length", visual.size_x);
            parse_float_attr(model, "Width", "width", visual.size_y);
            parse_float_attr(model, "Height", "height", visual.size_z);

            model_visual_by_name[lower_ascii(name)] = visual;
        }
    }

    if (tinyxml2::XMLElement *physical_descriptions = fixture_type->FirstChildElement("PhysicalDescriptions")) {
        if (tinyxml2::XMLElement *emitters = physical_descriptions->FirstChildElement("Emitters")) {
            for (tinyxml2::XMLElement *emitter = emitters->FirstChildElement("Emitter"); emitter;
                 emitter = emitter->NextSiblingElement("Emitter")) {
                const char *emitter_name = emitter->Attribute("Name");
                if (!emitter_name) {
                    emitter_name = emitter->Attribute("name");
                }
                if (!emitter_name) {
                    continue;
                }

                float dominant_wavelength = 0.0F;
                if (parse_float_attr(emitter, "DominantWaveLength", "dominantwavelength",
                                     dominant_wavelength)) {
                    emitter_wavelength_by_name[emitter_name] = dominant_wavelength;
                }
            }
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
                       const Matrix *, bool)>
        append_geometry;
    append_geometry = [&](tinyxml2::XMLElement *geometry, const std::string &geometry_parent_id,
                          const Matrix &geometry_parent_world, const char *override_model,
                          const Matrix *prepend_local, bool parent_is_lens) {
        const std::string geometry_tag = geometry->Name() ? geometry->Name() : "";
        Matrix local = parse_local_matrix(geometry);
        if (prepend_local) {
            local = MatrixUtils::Multiply(*prepend_local, local);
        }
        Matrix world = MatrixUtils::Multiply(geometry_parent_world, local);

        if (lower_ascii(geometry_tag) == "geometryreference") {
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
                            reference_model ? reference_model : override_model, &local, parent_is_lens);
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
        node.node_class = "fixture_geometry";
        node.is_fixture = true;
        node.is_axis = looks_like_axis(geometry->Name(), geometry_name);
        node.is_lens = looks_like_lens_geometry(geometry_tag, geometry_name, parent_is_lens);
        node.is_emitter = looks_like_emitter(geometry->Name(), geometry_name, node.is_lens);
        node.local_transform = peraviz::coordinate_mapper::to_godot_transform(local);

        if (lower_ascii(geometry_tag) == "beam") {
            node.has_luminous_flux = parse_float_attr(geometry, "LuminousFlux", "luminousflux",
                                                      node.luminous_flux);
            node.has_color_temperature = parse_float_attr(
                geometry, "ColorTemperature", "colortemperature", node.color_temperature);
            node.has_beam_angle = parse_float_attr(geometry, "BeamAngle", "beamangle",
                                                   node.beam_angle);
            node.has_field_angle = parse_float_attr(geometry, "FieldAngle", "fieldangle",
                                                    node.field_angle);
            node.has_beam_radius = parse_float_attr(geometry, "BeamRadius", "beamradius",
                                                    node.beam_radius);

            const char *emitter_spectrum = geometry->Attribute("EmitterSpectrum");
            if (!emitter_spectrum) {
                emitter_spectrum = geometry->Attribute("emitterspectrum");
            }
            if (emitter_spectrum) {
                auto emitter_it = emitter_wavelength_by_name.find(emitter_spectrum);
                if (emitter_it != emitter_wavelength_by_name.end()) {
                    node.has_dominant_wavelength = true;
                    node.dominant_wavelength = emitter_it->second;
                }
            }
        }

        if (node.is_emitter) {
            peraviz::debug_runtime::log_coordinate_debug_event(
                "emitter_reference", request.fixture_name,
                "geometry_id=" + geometry_id +
                    " expected_beam_direction_local=-Z (GDTF reference)");
        }

        const char *model_name = override_model ? override_model : geometry->Attribute("Model");
        if (!model_name) {
            model_name = geometry->Attribute("model");
        }
        if (model_name) {
            auto model_it = model_visual_by_name.find(lower_ascii(model_name));
            if (model_it != model_visual_by_name.end()) {
                if (!model_it->second.file.empty()) {
                    node.asset_path = gdtf_cache.ensure_gdtf_model_extracted(model_it->second.file);
                }
                node.primitive_type = model_it->second.primitive_type;
                node.primitive_size_x = std::max(model_it->second.size_x, 0.001F);
                node.primitive_size_y = std::max(model_it->second.size_y, 0.001F);
                node.primitive_size_z = std::max(model_it->second.size_z, 0.001F);
            }
        }
        node.asset_kind = infer_asset_kind_from_path(node.asset_path);
        if (node.asset_kind == "none" && !node.primitive_type.empty()) {
            node.asset_kind = "primitive";
        }

        nodes.push_back(node);

        for (tinyxml2::XMLElement *child = geometry->FirstChildElement(); child;
             child = child->NextSiblingElement()) {
            const std::string child_tag = child->Name() ? child->Name() : "";
            if (!is_supported_geometry_tag(child_tag)) {
                continue;
            }
            append_geometry(child, geometry_id, world, nullptr, nullptr, node.is_lens);
        }
    };

    append_geometry(root_geometry, parent_id, parent_world, nullptr, nullptr, false);
    extracted_asset_count += gdtf_cache.extracted_assets();
    return nodes;
}

} // namespace peraviz
