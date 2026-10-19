/* This file is part of Perastage. */
#include "mvr_matrix_precision.h"
#include "matrixutils.h"
#include <tinyxml2.h>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string_view>

namespace mvr {
namespace {
bool Equal(const Matrix &a, const Matrix &b) {
  return a.u == b.u && a.v == b.v && a.w == b.w && a.o == b.o;
}
std::string Format(const std::array<double, 12> &values) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::setprecision(std::numeric_limits<double>::max_digits10);
  for (size_t row = 0; row < 4; ++row) {
    stream << '{';
    for (size_t col = 0; col < 3; ++col) {
      if (col) stream << ',';
      // Normalize signed zero; it has no affine placement significance.
      stream << (values[row * 3 + col] == 0 ? 0.0 : values[row * 3 + col]);
    }
    stream << '}';
  }
  return stream.str();
}
// Historical Perastage TrussInfo v1 files used one flat comma-separated list.
// This compatibility boundary never relaxes the canonical MVR matrix parser.
std::optional<MvrMatrixSource> ReadLegacyTrussMatrix(const tinyxml2::XMLElement &node,
    const tinyxml2::XMLElement &element, const MvrScene &scene) {
  const auto owned = [](const char *value) {
    std::string provider = value ? value : "";
    std::transform(provider.begin(), provider.end(), provider.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return provider == "perastage";
  };
  if (std::string_view(node.Name()) != "Truss" || !owned(scene.provider.c_str()) ||
      element.FirstAttribute()) return std::nullopt;
  bool recognized = false;
  const auto *userData = node.FirstChildElement("UserData");
  for (auto *data = userData ? userData->FirstChildElement("Data") : nullptr;
       data; data = data->NextSiblingElement("Data"))
    recognized |= owned(data->Attribute("provider")) && data->Attribute("ver") &&
        std::string_view(data->Attribute("ver")) == "1.0" && data->FirstChildElement("TrussInfo");
  if (!recognized) return std::nullopt;
  std::string text;
  for (auto *child = element.FirstChild(); child; child = child->NextSibling()) {
    if (child->ToText()) text += child->Value();
    else if (!child->ToComment()) return std::nullopt;
  }
  std::istringstream input(text);
  input.imbue(std::locale::classic());
  std::array<double, 12> values;
  for (size_t index = 0; index < values.size(); ++index) {
    if (!(input >> values[index]) || !std::isfinite(values[index])) return std::nullopt;
    char comma;
    if (index + 1 < values.size() && (!(input >> comma) || comma != ',')) return std::nullopt;
  }
  input >> std::ws;
  if (!input.eof()) return std::nullopt;
  return ParseCanonicalMatrix(Format(values));
}

void Write(tinyxml2::XMLElement &node, const Matrix &edited,
           const std::optional<MvrMatrixSource> &source) {
  if (auto *matrix = node.FirstChildElement("Matrix"))
    matrix->SetText(FormatCanonicalMatrix(edited, source).c_str());
}
} // namespace

std::optional<MvrMatrixSource> ParseCanonicalMatrix(const std::string &text) {
  std::istringstream stream(text);
  stream.imbue(std::locale::classic());
  MvrMatrixSource source;
  for (size_t row = 0; row < 4; ++row) {
    char delimiter;
    if (!(stream >> delimiter) || delimiter != '{') return std::nullopt;
    for (size_t col = 0; col < 3; ++col) {
      auto &value = source.values[row * 3 + col];
      if (!(stream >> value) || !std::isfinite(value) ||
          std::abs(value) > std::numeric_limits<float>::max()) return std::nullopt;
      if (!(stream >> delimiter) || delimiter != (col == 2 ? '}' : ',')) return std::nullopt;
    }
  }
  stream >> std::ws;
  if (!stream.eof()) return std::nullopt;
  auto project = [&](size_t row) {
    return std::array<float, 3>{static_cast<float>(source.values[row * 3]),
        static_cast<float>(source.values[row * 3 + 1]), static_cast<float>(source.values[row * 3 + 2])};
  };
  source.projection.u = project(0); source.projection.v = project(1);
  source.projection.w = project(2); source.projection.o = project(3);
  return source;
}

std::optional<MvrMatrixSource> ReadCanonicalMatrixElement(const tinyxml2::XMLElement &element) {
  if (element.FirstAttribute()) return std::nullopt;
  std::string text;
  for (auto *child = element.FirstChild(); child; child = child->NextSibling()) {
    if (child->ToText()) text += child->Value();
    else if (!child->ToComment()) return std::nullopt;
  }
  return ParseCanonicalMatrix(text);
}

std::string FormatCanonicalMatrix(const Matrix &edited,
                                 const std::optional<MvrMatrixSource> &source) {
  if (source && Equal(edited, source->projection)) return Format(source->values);
  // Preserve untouched authored components; lift changed floats exactly. Both
  // paths use one canonical precision, including the first edited export.
  const Matrix &baseline = source ? source->projection : edited;
  const std::array rows{edited.u, edited.v, edited.w, edited.o};
  const std::array projected{baseline.u, baseline.v, baseline.w, baseline.o};
  std::array<double, 12> values;
  for (size_t row = 0; row < rows.size(); ++row)
    for (size_t col = 0; col < rows[row].size(); ++col) {
      const size_t index = row * 3 + col;
      values[index] = source && rows[row][col] == projected[row][col]
          ? source->values[index] : static_cast<double>(rows[row][col]);
    }
  return Format(values);
}
bool CanonicalMatrixIsIdentity(const Matrix &matrix,
                               const std::optional<MvrMatrixSource> &source) {
  const auto parsed = ParseCanonicalMatrix(FormatCanonicalMatrix(matrix, source));
  const auto identity = ParseCanonicalMatrix(MatrixUtils::FormatMatrix(Matrix{}));
  return parsed && parsed->values == identity->values;
}

std::optional<MvrMatrixSource> ReadMatrixSource(tinyxml2::XMLElement &node, MvrScene &scene) {
  auto *element = node.FirstChildElement("Matrix");
  if (!element) return ParseCanonicalMatrix("{1,0,0}{0,1,0}{0,0,1}{0,0,0}");
  auto source = ReadCanonicalMatrixElement(*element);
  if (!source) source = ReadLegacyTrussMatrix(node, *element, scene);
  if (source && !element->NextSiblingElement("Matrix")) return source;
  const auto *uuid = node.Attribute("uuid");
  scene.preservationIssues.push_back({"Matrix", uuid ? uuid : "",
      std::string(node.Name()) + "/Matrix", element->GetText() ? element->GetText() : "",
      "Matrix must contain four finite comma-separated triples within the editable numeric range, without duplicate nodes or extra structure.",
      "Correct the malformed source Matrix before exporting."});
  return std::nullopt;
}

void WriteCanonicalMatrices(tinyxml2::XMLElement &sceneElement, const MvrScene &scene) {
  const auto visit = [&](auto &&self, tinyxml2::XMLElement &node) -> void {
    const std::string_view kind = node.Name();
    if (kind == "UserData") return;
    if (const auto *uuid = node.Attribute("uuid")) {
      const auto object = [&](const auto &objects) {
        const auto found = objects.find(uuid);
        if (found == objects.end()) return;
        const auto &value = found->second;
        Write(node, value.parentGroupUuid.empty() ? value.transform : value.localTransform,
              value.matrixSource);
      };
      if (kind == "Truss") object(scene.trusses);
      else if (kind == "Fixture") object(scene.fixtures);
      else if (kind == "Support") object(scene.supports);
      else if (kind == "SceneObject" || kind == "FocusPoint") object(scene.sceneObjects);
      else if (kind == "GroupObject") {
        if (const auto found = scene.groupObjects.find(uuid); found != scene.groupObjects.end())
          Write(node, found->second.localTransform, found->second.matrixSource);
      } else if (kind == "Symdef") {
        const auto found = scene.symdefs.find(uuid);
        auto *children = node.FirstChildElement("ChildList");
        size_t index = 0;
        for (auto *geo = children ? children->FirstChildElement("Geometry3D") : nullptr;
             geo && found != scene.symdefs.end() && index < found->second.geometries.size();
             geo = geo->NextSiblingElement("Geometry3D"), ++index) {
          const auto &value = found->second.geometries[index];
          Write(*geo, value.transform, value.matrixSource);
        }
      }
    }
    for (auto *child = node.FirstChildElement(); child; child = child->NextSiblingElement()) self(self, *child);
  };
  visit(visit, sceneElement);
}
} // namespace mvr
