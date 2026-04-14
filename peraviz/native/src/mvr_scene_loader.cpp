#include "mvr_scene_loader.h"

#include "asset_cache.h"
#include "coordinate_mapper.h"
#include "gdtf_scene_builder.h"
#include "matrixutils.h"
#include "peraviz_debug_runtime.h"
#include "types.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <tinyxml2.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {

using peraviz::SceneModel;
using peraviz::SceneNode;

struct SymdefGeometry {
    std::string file_name;
    Matrix transform = MatrixUtils::Identity();
};

std::string lower_ascii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

bool is_element_name(tinyxml2::XMLElement *node, const char *expected_lower_ascii) {
    if (!node || !expected_lower_ascii) {
        return false;
    }
    return lower_ascii(node->Name() ? node->Name() : "") == expected_lower_ascii;
}

tinyxml2::XMLElement *first_child_element_ci(tinyxml2::XMLElement *parent,
                                             const char *expected_lower_ascii) {
    if (!parent || !expected_lower_ascii) {
        return nullptr;
    }
    for (tinyxml2::XMLElement *child = parent->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
        if (is_element_name(child, expected_lower_ascii)) {
            return child;
        }
    }
    return nullptr;
}

const char *child_text_ci(tinyxml2::XMLElement *parent,
                          const char *expected_lower_ascii) {
    if (tinyxml2::XMLElement *child = first_child_element_ci(parent, expected_lower_ascii)) {
        return child->GetText();
    }
    return nullptr;
}

std::string read_xml_from_mvr(const std::string &path) {
    wxFileInputStream input(wxString::FromUTF8(path.c_str()));
    if (!input.IsOk()) {
        return {};
    }

    wxZipInputStream zip(input);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zip.GetNextEntry())), entry) {
        std::string file_name = lower_ascii(entry->GetName().ToUTF8().data());
        if (file_name.find("generalscenedescription.xml") == std::string::npos) {
            continue;
        }

        std::string xml;
        char buffer[4096];
        while (!zip.Eof()) {
            zip.Read(buffer, sizeof(buffer));
            const size_t n = zip.LastRead();
            if (n == 0) {
                break;
            }
            xml.append(buffer, n);
        }
        return xml;
    }

    return {};
}

std::string node_id(tinyxml2::XMLElement *node, int serial) {
    if (const char *uuid = node->Attribute("uuid")) {
        return uuid;
    }
    if (const char *name = node->Attribute("name")) {
        return std::string(name) + "#" + std::to_string(serial);
    }
    return std::string(node->Name()) + "#" + std::to_string(serial);
}

Matrix parse_matrix_node(tinyxml2::XMLElement *node) {
    Matrix m = MatrixUtils::Identity();
    if (!node) {
        return m;
    }

    if (const char *matrix_attr = node->Attribute("Matrix")) {
        MatrixUtils::ParseMatrix(matrix_attr, m);
        return m;
    }
    if (const char *matrix_attr = node->Attribute("matrix")) {
        MatrixUtils::ParseMatrix(matrix_attr, m);
        return m;
    }

    if (tinyxml2::XMLElement *matrix_node = first_child_element_ci(node, "matrix")) {
        if (const char *text = matrix_node->GetText()) {
            MatrixUtils::ParseMatrix(text, m);
        }
    }
    return m;
}

std::string parse_model_filename(tinyxml2::XMLElement *geo_node) {
    if (!geo_node) {
        return {};
    }

    if (const char *file_name = geo_node->Attribute("fileName")) {
        return file_name;
    }
    if (const char *file_name = geo_node->Attribute("FileName")) {
        return file_name;
    }
    if (const char *file_name = geo_node->Attribute("file")) {
        return file_name;
    }
    if (const char *file_name = geo_node->Attribute("File")) {
        return file_name;
    }
    if (const char *model = geo_node->Attribute("model")) {
        return model;
    }
    if (const char *model = geo_node->Attribute("Model")) {
        return model;
    }
    if (const char *geometry = geo_node->Attribute("Geometry")) {
        return geometry;
    }
    if (const char *geometry = geo_node->Attribute("geometry")) {
        return geometry;
    }
    if (const char *text = child_text_ci(geo_node, "filename")) {
        return text;
    }
    if (const char *text = child_text_ci(geo_node, "file")) {
        return text;
    }
    return {};
}

std::string normalize_geometry_file_name(const std::string &file_name) {
    std::string normalized = file_name;
    const auto is_space = [](unsigned char c) {
        return std::isspace(c) != 0;
    };
    while (!normalized.empty() && is_space(static_cast<unsigned char>(normalized.front()))) {
        normalized.erase(normalized.begin());
    }
    while (!normalized.empty() && is_space(static_cast<unsigned char>(normalized.back()))) {
        normalized.pop_back();
    }
    if (normalized.empty()) {
        return {};
    }
    std::filesystem::path path = std::filesystem::u8path(normalized);
    return path.u8string();
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

std::string parse_name(tinyxml2::XMLElement *node, const std::string &fallback) {
    if (const char *name = node->Attribute("name")) {
        return name;
    }
    if (const char *name = node->Attribute("Name")) {
        return name;
    }
    return fallback;
}

std::string trim_ascii(std::string value) {
    const auto is_space = [](unsigned char c) {
        return std::isspace(c) != 0;
    };

    while (!value.empty() && is_space(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && is_space(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

int parse_int_text(const char *value) {
    if (!value) {
        return -1;
    }

    const std::string trimmed = trim_ascii(value);
    if (trimmed.empty()) {
        return -1;
    }

    char *end = nullptr;
    const long parsed = std::strtol(trimmed.c_str(), &end, 10);
    if (end == trimmed.c_str() || parsed <= 0L) {
        return -1;
    }
    return static_cast<int>(parsed);
}

bool try_parse_fixture_address_text(const std::string &text,
                                    int break_index_zero_based,
                                    int &out_universe,
                                    int &out_address) {
    const std::string trimmed = trim_ascii(text);
    if (trimmed.empty()) {
        return false;
    }

    const std::size_t dot = trimmed.find('.');
    if (dot != std::string::npos) {
        const int universe = parse_int_text(trimmed.substr(0, dot).c_str());
        const int address = parse_int_text(trimmed.substr(dot + 1).c_str());
        if (universe > 0 && address >= 1 && address <= 512) {
            out_universe = universe;
            out_address = address;
            return true;
        }
        return false;
    }

    const int absolute_or_channel = parse_int_text(trimmed.c_str());
    if (absolute_or_channel <= 0) {
        return false;
    }

    if (absolute_or_channel <= 512) {
        out_universe = std::max(1, break_index_zero_based + 1);
        out_address = absolute_or_channel;
        return true;
    }

    const int base_universe = std::max(1, break_index_zero_based + 1);
    out_universe = base_universe + ((absolute_or_channel - 1) / 512);
    out_address = ((absolute_or_channel - 1) % 512) + 1;
    return true;
}

int read_int_attribute(tinyxml2::XMLElement *node, std::initializer_list<const char *> names) {
    if (!node) {
        return -1;
    }
    for (const char *name : names) {
        const int parsed = parse_int_text(node->Attribute(name));
        if (parsed > 0) {
            return parsed;
        }
    }
    return -1;
}

int read_int_child_value(tinyxml2::XMLElement *node, std::initializer_list<const char *> names) {
    if (!node) {
        return -1;
    }
    for (const char *name : names) {
        if (tinyxml2::XMLElement *child = node->FirstChildElement(name)) {
            const int parsed = parse_int_text(child->GetText());
            if (parsed > 0) {
                return parsed;
            }
        }
    }

    for (tinyxml2::XMLElement *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
        const std::string child_name = lower_ascii(child->Name());
        for (const char *name : names) {
            const std::string expected_name = lower_ascii(name);
            if (child_name != expected_name) {
                continue;
            }
            const int parsed = parse_int_text(child->GetText());
            if (parsed > 0) {
                return parsed;
            }
        }
    }
    return -1;
}

SceneModel::FixturePatch parse_fixture_patch(tinyxml2::XMLElement *fixture_node,
                                             const std::string &fixture_uuid,
                                             peraviz::ZipAssetCache &mvr_cache) {
    SceneModel::FixturePatch patch;
    patch.fixture_uuid = fixture_uuid;

    std::string gdtf_spec;
    if (const char *spec_text = child_text_ci(fixture_node, "gdtfspec")) {
        gdtf_spec = spec_text;
    }
    if (!gdtf_spec.empty()) {
        patch.gdtf_path = mvr_cache.ensure_gdtf_spec_extracted(gdtf_spec);
    }

    if (const char *mode_text = child_text_ci(fixture_node, "gdtfmode")) {
        patch.dmx_mode = mode_text;
    }

    int universe = read_int_attribute(fixture_node, {"Universe", "universe", "DMXUniverse", "dmxUniverse"});
    int address = read_int_attribute(fixture_node, {"Address", "address", "DMXAddress", "dmxAddress"});
    int absolute_address = read_int_attribute(fixture_node, {"AbsoluteAddress", "absoluteAddress", "AbsDMXAddress", "absDmxAddress"});

    universe = universe > 0 ? universe : read_int_child_value(fixture_node, {"Universe", "DMXUniverse"});
    address = address > 0 ? address : read_int_child_value(fixture_node, {"Address", "DMXAddress"});
    absolute_address = absolute_address > 0
                           ? absolute_address
                           : read_int_child_value(fixture_node, {"AbsoluteAddress", "AbsDMXAddress", "DMXAbsoluteAddress"});

    if (tinyxml2::XMLElement *addresses = fixture_node->FirstChildElement("Addresses"); addresses) {
        for (tinyxml2::XMLElement *address_node = addresses->FirstChildElement("Address"); address_node;
             address_node = address_node->NextSiblingElement("Address")) {
            if (universe <= 0) {
                universe = read_int_attribute(address_node, {"Universe", "universe", "DMXUniverse"});
            }
            if (address <= 0) {
                address = read_int_attribute(address_node, {"Address", "address", "DMXAddress"});
            }
            if (absolute_address <= 0) {
                absolute_address = read_int_attribute(address_node, {"AbsoluteAddress", "absoluteAddress", "DMXAbsoluteAddress"});
            }

            int parsed_universe = -1;
            int parsed_address = -1;
            const int break_index = std::max(0, read_int_attribute(address_node, {"break", "Break"}));
            if (try_parse_fixture_address_text(address_node->GetText() ? address_node->GetText() : "",
                                               break_index,
                                               parsed_universe,
                                               parsed_address)) {
                if (universe <= 0) {
                    universe = parsed_universe;
                }
                if (address <= 0) {
                    address = parsed_address;
                }
                if (absolute_address <= 0 && parsed_universe > 0 && parsed_address > 0) {
                    absolute_address = ((parsed_universe - 1) * 512) + parsed_address;
                }
            }

            if (universe > 0 && address > 0) {
                break;
            }
        }
    }

    if (absolute_address > 0 && (universe <= 0 || address <= 0)) {
        universe = ((absolute_address - 1) / 512) + 1;
        address = ((absolute_address - 1) % 512) + 1;
    }

    patch.mvr_universe = universe;
    patch.mvr_address = address;
    return patch;
}

std::unordered_map<std::string, std::vector<SymdefGeometry>> parse_symdefs(tinyxml2::XMLElement *root) {
    std::unordered_map<std::string, std::vector<SymdefGeometry>> symdefs;

    tinyxml2::XMLElement *aux_data = first_child_element_ci(root, "auxdata");
    if (!aux_data) {
        if (tinyxml2::XMLElement *scene = first_child_element_ci(root, "scene")) {
            aux_data = first_child_element_ci(scene, "auxdata");
        }
    }
    if (!aux_data) {
        return symdefs;
    }

    for (tinyxml2::XMLElement *symdef = aux_data->FirstChildElement(); symdef;
         symdef = symdef->NextSiblingElement()) {
        if (!is_element_name(symdef, "symdef")) {
            continue;
        }
        const char *symdef_id = symdef->Attribute("uuid");
        const std::string symdef_id_normalized = trim_ascii(symdef_id ? symdef_id : "");
        if (symdef_id_normalized.empty()) {
            continue;
        }

        std::vector<SymdefGeometry> geometries;
        std::function<void(tinyxml2::XMLElement *, const Matrix &)> parse_child_list;
        parse_child_list = [&](tinyxml2::XMLElement *node, const Matrix &parent_world) {
            for (tinyxml2::XMLElement *child = node->FirstChildElement(); child;
                 child = child->NextSiblingElement()) {
                Matrix local = parse_matrix_node(child);
                Matrix world = MatrixUtils::Multiply(parent_world, local);

                if (is_element_name(child, "geometry3d")) {
                    const std::string model_name = normalize_geometry_file_name(parse_model_filename(child));
                    if (!model_name.empty()) {
                        SymdefGeometry geometry;
                        geometry.file_name = model_name;
                        geometry.transform = world;
                        geometries.push_back(std::move(geometry));
                    }
                }

                if (tinyxml2::XMLElement *inner = first_child_element_ci(child, "childlist")) {
                    parse_child_list(inner, world);
                }
            }
        };

        if (tinyxml2::XMLElement *child_list = first_child_element_ci(symdef, "childlist")) {
            parse_child_list(child_list, MatrixUtils::Identity());
        }

        if (tinyxml2::XMLElement *geometries_node = first_child_element_ci(symdef, "geometries")) {
            parse_child_list(geometries_node, MatrixUtils::Identity());
        }

        parse_child_list(symdef, MatrixUtils::Identity());
        if (!geometries.empty()) {
            symdefs[lower_ascii(symdef_id_normalized)] = std::move(geometries);
        }
    }

    return symdefs;
}

void append_scene_node(SceneModel &scene, SceneNode node) {
    if (node.type == "fixture") {
        ++scene.fixture_count;
    } else if (node.type == "truss") {
        ++scene.truss_count;
    } else if (node.type == "support") {
        ++scene.support_count;
    } else if (node.type == "scene_object") {
        ++scene.object_count;
    }
    scene.nodes.push_back(std::move(node));
}

int append_single_geometry(SceneModel &scene, tinyxml2::XMLElement *geo,
                           const std::string &parent_id, const Matrix &parent_world,
                           peraviz::ZipAssetCache &mvr_cache, const std::string &prefix,
                           int &serial) {
    if (!geo || !is_element_name(geo, "geometry3d")) {
        return 0;
    }

    Matrix local = parse_matrix_node(geo);
    Matrix world = MatrixUtils::Multiply(parent_world, local);

    SceneNode geo_node;
    geo_node.node_id = prefix + "/geometry#" + std::to_string(serial++);
    geo_node.parent_id = parent_id;
    geo_node.name = parse_name(geo, "Geometry3D");
    geo_node.type = "model_part";
    geo_node.node_class = "model_part";
    geo_node.local_transform = peraviz::coordinate_mapper::to_godot_transform(local);
    const std::string model_name = normalize_geometry_file_name(parse_model_filename(geo));
    if (!model_name.empty()) {
        geo_node.asset_path = mvr_cache.ensure_mvr_model_extracted(model_name);
    }
    geo_node.asset_kind = infer_asset_kind_from_path(geo_node.asset_path);
    scene.nodes.push_back(std::move(geo_node));
    (void)world;
    return 1;
}

void append_geometry_children(SceneModel &scene, tinyxml2::XMLElement *node, const std::string &parent_id,
                              const Matrix &parent_world, peraviz::ZipAssetCache &mvr_cache,
                              const std::unordered_map<std::string, std::vector<SymdefGeometry>> &symdefs,
                              const std::string &prefix, int &serial) {
    int appended_count = 0;

    if (tinyxml2::XMLElement *geometries = first_child_element_ci(node, "geometries")) {
        for (tinyxml2::XMLElement *geo = geometries->FirstChildElement(); geo;
             geo = geo->NextSiblingElement()) {
            appended_count += append_single_geometry(scene, geo, parent_id, parent_world,
                                                     mvr_cache, prefix, serial);
        }

        for (tinyxml2::XMLElement *symbol = geometries->FirstChildElement(); symbol;
             symbol = symbol->NextSiblingElement()) {
            if (!is_element_name(symbol, "symbol")) {
                continue;
            }

            const char *symdef_attr = symbol->Attribute("symdef");
            if (!symdef_attr) {
                symdef_attr = symbol->Attribute("SymDef");
            }

            Matrix symbol_local = parse_matrix_node(symbol);
            const std::string symdef_lookup =
                symdef_attr ? lower_ascii(trim_ascii(symdef_attr)) : "";
            auto sym_it = symdefs.find(symdef_lookup);
            if (sym_it != symdefs.end()) {
                for (const SymdefGeometry &sym_geo : sym_it->second) {
                    SceneNode symbol_node;
                    symbol_node.node_id = prefix + "/symbol#" + std::to_string(serial++);
                    symbol_node.parent_id = parent_id;
                    symbol_node.name = parse_name(symbol, "Symbol");
                    symbol_node.type = "model_part";
                    symbol_node.node_class = "model_part";

                    Matrix local = MatrixUtils::Multiply(symbol_local, sym_geo.transform);
                    symbol_node.local_transform = peraviz::coordinate_mapper::to_godot_transform(local);

                    if (!sym_geo.file_name.empty()) {
                        symbol_node.asset_path = mvr_cache.ensure_mvr_model_extracted(sym_geo.file_name);
                    }
                    symbol_node.asset_kind = infer_asset_kind_from_path(symbol_node.asset_path);
                    scene.nodes.push_back(std::move(symbol_node));
                    ++appended_count;
                }
                continue;
            }

            // Compatibility fallback for exports that attach a direct model to
            // Symbol without an AUXData Symdef definition.
            const std::string fallback_model =
                normalize_geometry_file_name(parse_model_filename(symbol));
            if (!fallback_model.empty()) {
                SceneNode symbol_node;
                symbol_node.node_id = prefix + "/symbol#" + std::to_string(serial++);
                symbol_node.parent_id = parent_id;
                symbol_node.name = parse_name(symbol, "Symbol");
                symbol_node.type = "model_part";
                symbol_node.node_class = "model_part";
                symbol_node.local_transform =
                    peraviz::coordinate_mapper::to_godot_transform(symbol_local);
                symbol_node.asset_path = mvr_cache.ensure_mvr_model_extracted(fallback_model);
                symbol_node.asset_kind = infer_asset_kind_from_path(symbol_node.asset_path);
                scene.nodes.push_back(std::move(symbol_node));
                ++appended_count;
            }
        }
    }

    // Some MVR exporters emit Geometry3D directly under the node instead of
    // wrapping them in Geometries.
    for (tinyxml2::XMLElement *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
        if (is_element_name(child, "geometry3d")) {
            appended_count += append_single_geometry(scene, child, parent_id, parent_world,
                                                     mvr_cache, prefix, serial);
        }
    }

    if (appended_count == 0) {
        const std::string direct_model = normalize_geometry_file_name(parse_model_filename(node));
        if (!direct_model.empty()) {
            SceneNode node_model;
            node_model.node_id = prefix + "/geometry#" + std::to_string(serial++);
            node_model.parent_id = parent_id;
            node_model.name = parse_name(node, "Geometry3D");
            node_model.type = "model_part";
            node_model.node_class = "model_part";
            node_model.local_transform =
                peraviz::coordinate_mapper::to_godot_transform(MatrixUtils::Identity());
            node_model.asset_path = mvr_cache.ensure_mvr_model_extracted(direct_model);
            node_model.asset_kind = infer_asset_kind_from_path(node_model.asset_path);
            scene.nodes.push_back(std::move(node_model));
        }
    }
}

} // namespace

namespace peraviz {

SceneModel load_mvr(const std::string &path, bool peraviz_debug_baseline,
                    bool peraviz_debug_coords) {
    peraviz::debug_runtime::set_baseline_debug_enabled(peraviz_debug_baseline);
    peraviz::debug_runtime::set_coordinate_debug_enabled(peraviz_debug_coords);
    peraviz::debug_runtime::log_coordinate_mapping_metadata();

    SceneModel model;
    if (!std::filesystem::exists(std::filesystem::u8path(path))) {
        return model;
    }

    ZipAssetCache mvr_cache(path);
    model.cache_path = mvr_cache.cache_dir().u8string();

    const std::string xml_content = read_xml_from_mvr(path);
    if (xml_content.empty()) {
        return model;
    }

    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml_content.c_str()) != tinyxml2::XML_SUCCESS) {
        return model;
    }

    auto *root = doc.FirstChildElement("GeneralSceneDescription");
    if (!root) {
        root = doc.RootElement();
        if (!is_element_name(root, "generalscenedescription")) {
            return model;
        }
    }
    auto *scene = first_child_element_ci(root, "scene");
    if (!scene) {
        return model;
    }
    auto *layers = first_child_element_ci(scene, "layers");
    if (!layers) {
        return model;
    }

    const auto symdefs = parse_symdefs(root);

    int serial = 0;
    std::function<void(tinyxml2::XMLElement *, const Matrix &, const std::string &)> parse_child_list;
    parse_child_list = [&](tinyxml2::XMLElement *child_list, const Matrix &parent_world,
                           const std::string &parent_id) {
        for (tinyxml2::XMLElement *child = child_list->FirstChildElement(); child;
             child = child->NextSiblingElement()) {
            Matrix local_transform = parse_matrix_node(child);
            Matrix node_world = MatrixUtils::Multiply(parent_world, local_transform);
            const std::string node_name = child->Name();
            const std::string node_name_lower = lower_ascii(node_name);
            const std::string id = node_id(child, serial++);

            SceneNode node;
            node.node_id = id;
            node.parent_id = parent_id;
            node.name = parse_name(child, node_name);
            node.local_transform = peraviz::coordinate_mapper::to_godot_transform(local_transform);

            peraviz::debug_runtime::log_coordinate_debug_event(
                "instantiate_node", node_name,
                "node_id=" + id + " parent_id=" + parent_id + " name=" + node.name);

            if (node_name_lower == "fixture") {
                node.type = "fixture";
                node.node_class = "fixture";
                node.asset_kind = "none";
                node.is_fixture = true;
                append_scene_node(model, node);

                const SceneModel::FixturePatch fixture_patch = parse_fixture_patch(child, id, mvr_cache);
                model.fixture_patches.push_back(fixture_patch);

                const std::string gdtf_mode = fixture_patch.dmx_mode;
                const std::string gdtf_path = fixture_patch.gdtf_path;

                if (!gdtf_path.empty()) {
                    const GdtfBuildRequest request{gdtf_path, gdtf_mode, id, node.name};
                    auto fixture_nodes = build_fixture_geometry_nodes(request, id, node_world,
                                                                      model.extracted_asset_count);
                    model.nodes.insert(model.nodes.end(), fixture_nodes.begin(), fixture_nodes.end());
                }
            } else if (node_name_lower == "truss") {
                node.type = "truss";
                node.node_class = "truss";
                node.asset_kind = "none";
                append_scene_node(model, node);
                append_geometry_children(model, child, id, node_world, mvr_cache, symdefs, id, serial);
            } else if (node_name_lower == "support") {
                node.type = "support";
                node.node_class = "support";
                node.asset_kind = "none";
                append_scene_node(model, node);
                append_geometry_children(model, child, id, node_world, mvr_cache, symdefs, id, serial);
            } else if (node_name_lower == "sceneobject") {
                node.type = "scene_object";
                node.node_class = "scene_object";
                node.asset_kind = "none";
                append_scene_node(model, node);
                append_geometry_children(model, child, id, node_world, mvr_cache, symdefs, id, serial);
            }

            if (tinyxml2::XMLElement *nested = first_child_element_ci(child, "childlist")) {
                parse_child_list(nested, node_world, id);
            }
        }
    };

    for (tinyxml2::XMLElement *root_list = layers->FirstChildElement(); root_list;
         root_list = root_list->NextSiblingElement()) {
        if (is_element_name(root_list, "childlist")) {
            parse_child_list(root_list, MatrixUtils::Identity(), "");
        }
    }

    for (tinyxml2::XMLElement *layer = layers->FirstChildElement(); layer;
         layer = layer->NextSiblingElement()) {
        if (!is_element_name(layer, "layer")) {
            continue;
        }
        if (tinyxml2::XMLElement *child_list = first_child_element_ci(layer, "childlist")) {
            parse_child_list(child_list, MatrixUtils::Identity(), "");
        }
    }

    model.extracted_asset_count += mvr_cache.extracted_assets();
    return model;
}

} // namespace peraviz
