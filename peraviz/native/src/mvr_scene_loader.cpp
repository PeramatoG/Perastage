#include "mvr_scene_loader.h"

#include "matrixutils.h"
#include "types.h"

#include <cmath>
#include <memory>
#include <cctype>
#include <filesystem>
#include <functional>
#include <string>

#include <tinyxml2.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {

using peraviz::SceneInstance;
using peraviz::SceneModel;
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

std::string read_xml_from_mvr(const std::string &path) {
    wxFileInputStream input(wxString::FromUTF8(path.c_str()));
    if (!input.IsOk()) {
        return {};
    }

    wxZipInputStream zip(input);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zip.GetNextEntry())), entry) {
        std::string file_name = entry->GetName().ToUTF8().data();
        std::string lower = file_name;
        for (char &c : lower) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (lower.find("generalscenedescription.xml") == std::string::npos) {
            continue;
        }

        std::string xml;
        char buffer[4096];
        while (!zip.Eof()) {
            zip.Read(buffer, sizeof(buffer));
            size_t n = zip.LastRead();
            if (n == 0) {
                break;
            }
            xml.append(buffer, n);
        }
        return xml;
    }

    return {};
}

void append_instance(SceneModel &scene, const std::string &type, bool is_fixture,
                     const std::string &id, const Matrix &world) {
    SceneInstance instance;
    instance.type = type;
    instance.is_fixture = is_fixture;
    instance.id = id.empty() ? type : id;

    instance.transform.position = map_position(world.o);

    Matrix basis = to_godot_basis_matrix(world);
    const auto scale = extract_scale(basis);
    instance.transform.scale = {scale[0], scale[1], scale[2]};

    Matrix rotation_only = normalize_basis(basis, scale);
    const auto euler = MatrixUtils::MatrixToEuler(rotation_only);
    instance.transform.rotation_degrees = {euler[0], euler[1], euler[2]};

    scene.instances.push_back(instance);
    if (type == "fixture") {
        ++scene.fixture_count;
    } else if (type == "truss") {
        ++scene.truss_count;
    } else if (type == "support") {
        ++scene.support_count;
    } else {
        ++scene.object_count;
    }
}

std::string node_id(tinyxml2::XMLElement *node) {
    if (const char *uuid = node->Attribute("uuid")) {
        return uuid;
    }
    if (const char *name = node->Attribute("name")) {
        return name;
    }
    return node->Name();
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

} // namespace

namespace peraviz {

SceneModel load_mvr(const std::string &path) {
    SceneModel model;
    if (!std::filesystem::exists(std::filesystem::u8path(path))) {
        return model;
    }

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

    std::function<void(tinyxml2::XMLElement *, const Matrix &)> parse_child_list;
    parse_child_list = [&](tinyxml2::XMLElement *child_list, const Matrix &parent) {
        for (tinyxml2::XMLElement *child = child_list->FirstChildElement(); child;
             child = child->NextSiblingElement()) {
            Matrix node_transform = MatrixUtils::Multiply(parent, parse_matrix_node(child));
            const std::string node_name = child->Name();
            if (node_name == "Fixture") {
                append_instance(model, "fixture", true, node_id(child), node_transform);
            } else if (node_name == "Truss") {
                append_instance(model, "truss", false, node_id(child), node_transform);
            } else if (node_name == "Support") {
                append_instance(model, "support", false, node_id(child), node_transform);
            } else if (node_name == "SceneObject") {
                append_instance(model, "scene_object", false, node_id(child), node_transform);
            }

            if (tinyxml2::XMLElement *nested = child->FirstChildElement("ChildList")) {
                parse_child_list(nested, node_transform);
            }
        }
    };

    for (tinyxml2::XMLElement *root_list = layers->FirstChildElement("ChildList"); root_list;
         root_list = root_list->NextSiblingElement("ChildList")) {
        parse_child_list(root_list, MatrixUtils::Identity());
    }

    for (tinyxml2::XMLElement *layer = layers->FirstChildElement("Layer"); layer;
         layer = layer->NextSiblingElement("Layer")) {
        if (tinyxml2::XMLElement *child_list = layer->FirstChildElement("ChildList")) {
            parse_child_list(child_list, parse_matrix_node(layer));
        }
    }

    return model;
}

} // namespace peraviz
