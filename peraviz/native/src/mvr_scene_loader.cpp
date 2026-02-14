#include "mvr_scene_loader.h"

#include "asset_cache.h"
#include "gdtf_scene_builder.h"
#include "matrixutils.h"
#include "types.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
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
using peraviz::Vec3;

struct SymdefGeometry {
    std::string file_name;
    Matrix transform = MatrixUtils::Identity();
};

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
    transform.basis_x = {basis.u[0], basis.u[1], basis.u[2]};
    transform.basis_y = {basis.v[0], basis.v[1], basis.v[2]};
    transform.basis_z = {basis.w[0], basis.w[1], basis.w[2]};
    transform.has_basis = true;

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
    if (tinyxml2::XMLElement *matrix_node = node->FirstChildElement("Matrix")) {
        if (const char *text = matrix_node->GetText()) {
            MatrixUtils::ParseMatrix(text, m);
        }
    }
    return m;
}

std::string parse_model_filename(tinyxml2::XMLElement *geo_node) {
    if (const char *file_name = geo_node->Attribute("fileName")) {
        return file_name;
    }
    if (const char *file_name = geo_node->Attribute("FileName")) {
        return file_name;
    }
    return {};
}

std::string normalize_geometry_file_name(const std::string &file_name) {
    if (file_name.empty()) {
        return {};
    }
    std::filesystem::path path = std::filesystem::u8path(file_name);
    if (!path.has_extension()) {
        path += ".3ds";
    }
    return path.u8string();
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

std::unordered_map<std::string, std::vector<SymdefGeometry>> parse_symdefs(tinyxml2::XMLElement *root) {
    std::unordered_map<std::string, std::vector<SymdefGeometry>> symdefs;

    tinyxml2::XMLElement *aux_data = root ? root->FirstChildElement("AUXData") : nullptr;
    if (!aux_data) {
        return symdefs;
    }

    for (tinyxml2::XMLElement *symdef = aux_data->FirstChildElement("Symdef"); symdef;
         symdef = symdef->NextSiblingElement("Symdef")) {
        const char *symdef_id = symdef->Attribute("uuid");
        if (!symdef_id) {
            continue;
        }

        tinyxml2::XMLElement *child_list = symdef->FirstChildElement("ChildList");
        if (!child_list) {
            continue;
        }

        std::vector<SymdefGeometry> geometries;
        std::function<void(tinyxml2::XMLElement *, const Matrix &)> parse_child_list;
        parse_child_list = [&](tinyxml2::XMLElement *node, const Matrix &parent_world) {
            for (tinyxml2::XMLElement *child = node->FirstChildElement(); child;
                 child = child->NextSiblingElement()) {
                Matrix local = parse_matrix_node(child);
                Matrix world = MatrixUtils::Multiply(parent_world, local);

                if (std::string(child->Name()) == "Geometry3D") {
                    const std::string model_name = normalize_geometry_file_name(parse_model_filename(child));
                    if (!model_name.empty()) {
                        SymdefGeometry geometry;
                        geometry.file_name = model_name;
                        geometry.transform = world;
                        geometries.push_back(std::move(geometry));
                    }
                }

                if (tinyxml2::XMLElement *inner = child->FirstChildElement("ChildList")) {
                    parse_child_list(inner, world);
                }
            }
        };

        parse_child_list(child_list, MatrixUtils::Identity());
        if (!geometries.empty()) {
            symdefs[symdef_id] = std::move(geometries);
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

void append_geometry_children(SceneModel &scene, tinyxml2::XMLElement *node, const std::string &parent_id,
                              const Matrix &parent_world, peraviz::ZipAssetCache &mvr_cache,
                              const std::unordered_map<std::string, std::vector<SymdefGeometry>> &symdefs,
                              const std::string &prefix, int &serial) {
    tinyxml2::XMLElement *geometries = node->FirstChildElement("Geometries");
    if (!geometries) {
        return;
    }

    for (tinyxml2::XMLElement *geo = geometries->FirstChildElement("Geometry3D"); geo;
         geo = geo->NextSiblingElement("Geometry3D")) {
        Matrix local = parse_matrix_node(geo);
        Matrix world = MatrixUtils::Multiply(parent_world, local);

        SceneNode geo_node;
        geo_node.node_id = prefix + "/geometry#" + std::to_string(serial++);
        geo_node.parent_id = parent_id;
        geo_node.name = parse_name(geo, "Geometry3D");
        geo_node.type = "model_part";
        geo_node.local_transform = to_godot_transform(local);
        const std::string model_name = normalize_geometry_file_name(parse_model_filename(geo));
        if (!model_name.empty()) {
            geo_node.asset_path = mvr_cache.ensure_extracted(model_name);
        }

        scene.nodes.push_back(std::move(geo_node));
        (void)world;
    }

    for (tinyxml2::XMLElement *symbol = geometries->FirstChildElement("Symbol"); symbol;
         symbol = symbol->NextSiblingElement("Symbol")) {
        const char *symdef_attr = symbol->Attribute("symdef");
        if (!symdef_attr) {
            continue;
        }

        Matrix symbol_local = parse_matrix_node(symbol);
        auto sym_it = symdefs.find(symdef_attr);
        if (sym_it == symdefs.end()) {
            continue;
        }

        for (const SymdefGeometry &sym_geo : sym_it->second) {
            SceneNode symbol_node;
            symbol_node.node_id = prefix + "/symbol#" + std::to_string(serial++);
            symbol_node.parent_id = parent_id;
            symbol_node.name = parse_name(symbol, "Symbol");
            symbol_node.type = "model_part";

            Matrix local = MatrixUtils::Multiply(symbol_local, sym_geo.transform);
            symbol_node.local_transform = to_godot_transform(local);

            if (!sym_geo.file_name.empty()) {
                symbol_node.asset_path = mvr_cache.ensure_extracted(sym_geo.file_name);
            }
            scene.nodes.push_back(std::move(symbol_node));
        }
    }
}

} // namespace

namespace peraviz {

SceneModel load_mvr(const std::string &path) {
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
        return model;
    }
    auto *scene = root->FirstChildElement("Scene");
    if (!scene) {
        return model;
    }
    auto *layers = scene->FirstChildElement("Layers");
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
            const std::string id = node_id(child, serial++);

            SceneNode node;
            node.node_id = id;
            node.parent_id = parent_id;
            node.name = parse_name(child, node_name);
            node.local_transform = to_godot_transform(local_transform);

            if (node_name == "Fixture") {
                node.type = "fixture";
                node.is_fixture = true;
                append_scene_node(model, node);

                std::string gdtf_spec;
                if (tinyxml2::XMLElement *gdtf = child->FirstChildElement("GDTFSpec"); gdtf && gdtf->GetText()) {
                    gdtf_spec = gdtf->GetText();
                }
                std::string gdtf_mode;
                if (tinyxml2::XMLElement *mode = child->FirstChildElement("GDTFMode"); mode && mode->GetText()) {
                    gdtf_mode = mode->GetText();
                }

                if (!gdtf_spec.empty()) {
                    const std::string gdtf_path = mvr_cache.ensure_extracted(gdtf_spec);
                    if (!gdtf_path.empty()) {
                        const GdtfBuildRequest request{gdtf_path, gdtf_mode, id, node.name};
                        auto fixture_nodes = build_fixture_geometry_nodes(request, id, node_world,
                                                                          model.extracted_asset_count);
                        model.nodes.insert(model.nodes.end(), fixture_nodes.begin(), fixture_nodes.end());
                    }
                }
            } else if (node_name == "Truss") {
                node.type = "truss";
                append_scene_node(model, node);
                append_geometry_children(model, child, id, node_world, mvr_cache, symdefs, id, serial);
            } else if (node_name == "Support") {
                node.type = "support";
                append_scene_node(model, node);
                append_geometry_children(model, child, id, node_world, mvr_cache, symdefs, id, serial);
            } else if (node_name == "SceneObject") {
                node.type = "scene_object";
                append_scene_node(model, node);
                append_geometry_children(model, child, id, node_world, mvr_cache, symdefs, id, serial);
            }

            if (tinyxml2::XMLElement *nested = child->FirstChildElement("ChildList")) {
                parse_child_list(nested, node_world, id);
            }
        }
    };

    for (tinyxml2::XMLElement *root_list = layers->FirstChildElement("ChildList"); root_list;
         root_list = root_list->NextSiblingElement("ChildList")) {
        parse_child_list(root_list, MatrixUtils::Identity(), "");
    }

    for (tinyxml2::XMLElement *layer = layers->FirstChildElement("Layer"); layer;
         layer = layer->NextSiblingElement("Layer")) {
        if (tinyxml2::XMLElement *child_list = layer->FirstChildElement("ChildList")) {
            parse_child_list(child_list, parse_matrix_node(layer), "");
        }
    }

    model.extracted_asset_count += mvr_cache.extracted_assets();
    return model;
}

} // namespace peraviz
