#include "symbols/PerastageSvgSymbol.h"
#include "fixture_symbol_resolution.h"
#include "gdtf_archive_reader.h"
#include "filesystem_path_utils.h"
#include "startup_file_access_gate.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string_view>
#include <utility>

#include <tinyxml2.h>

namespace {
std::string NormalizeArchivePath(std::string value) {
  std::replace(value.begin(), value.end(), '\\', '/');
  while (!value.empty() && value.front() == '/')
    value.erase(value.begin());
  return value;
}

bool EqualsNoCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

bool HasLegacySymbolRevisionForView(const tinyxml2::XMLElement *fixtureType,
                                    SymbolViewKind view) {
  if (!fixtureType)
    return false;
  const tinyxml2::XMLElement *revisions = fixtureType->FirstChildElement("Revisions");
  if (!revisions)
    return false;
  bool legacy = false;
  for (const tinyxml2::XMLElement *revision =
           revisions->FirstChildElement("Revision");
       revision; revision = revision->NextSiblingElement("Revision")) {
    const char *modifiedBy = revision->Attribute("ModifiedBy");
    const char *text = revision->Attribute("Text");
    if (!modifiedBy || !text)
      continue;
    if (IsPerastageFixtureSymbolRevisionForView(modifiedBy, text, view))
      legacy = true;
    if (IsPerastageStandardSvgMutationRevisionForView(modifiedBy, text, view) ||
        (view != SymbolViewKind::Bottom && view != SymbolViewKind::Back &&
         IsPerastageStandardSvgCleanupRevision(modifiedBy, text)))
      legacy = false;
  }
  // Revisions are appended in operation order. A later standard publication
  // supersedes only recognized older legacy ownership; stored SVG markers still
  // identify private compatibility content independently below.
  return legacy;
}

const tinyxml2::XMLElement *ResolveFixtureType(const tinyxml2::XMLDocument &doc) {
  const tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  if (fixtureType)
    fixtureType = fixtureType->FirstChildElement("FixtureType");
  if (!fixtureType)
    fixtureType = doc.FirstChildElement("FixtureType");
  return fixtureType;
}

const tinyxml2::XMLElement *ResolveTargetModel(const tinyxml2::XMLElement *fixtureType) {
  if (!fixtureType)
    return nullptr;
  const tinyxml2::XMLElement *models = fixtureType->FirstChildElement("Models");
  if (!models)
    return nullptr;

  const tinyxml2::XMLElement *targetModel = nullptr;
  for (const tinyxml2::XMLElement *model = models->FirstChildElement("Model"); model;
       model = model->NextSiblingElement("Model")) {
    const char *name = model->Attribute("Name");
    if (name && std::string(name) == "Main")
      return model;
    if (!targetModel)
      targetModel = model;
  }
  return targetModel;
}

std::string ResolveModelSvgBasename(const tinyxml2::XMLElement *targetModel) {
  if (!targetModel)
    return "main";
  const char *fileAttr = targetModel->Attribute("File");
  if (fileAttr && *fileAttr)
    return fileAttr;
  const char *nameAttr = targetModel->Attribute("Name");
  if (nameAttr && *nameAttr)
    return nameAttr;
  return "main";
}

std::vector<std::string> BuildSvgBaseNameCandidates(const std::string &baseName) {
  std::vector<std::string> candidates;
  if (baseName.empty()) {
    candidates.push_back("main");
    return candidates;
  }

  candidates.push_back(baseName);

  const std::filesystem::path basePath(baseName);
  const std::string stem = basePath.stem().string();
  if (!stem.empty() && stem != baseName)
    candidates.push_back(stem);

  return candidates;
}

bool ParseDoubles(const char *text, std::vector<double> &out) {
  if (!text)
    return false;
  std::string normalized(text);
  std::replace(normalized.begin(), normalized.end(), ',', ' ');
  std::string_view view(normalized);
  out.clear();

  while (!view.empty()) {
    while (!view.empty() && std::isspace(static_cast<unsigned char>(view.front())))
      view.remove_prefix(1);
    if (view.empty())
      break;
    const char *begin = view.data();
    char *end = nullptr;
    double value = std::strtod(begin, &end);
    if (begin == end)
      break;
    out.push_back(value);
    view.remove_prefix(static_cast<size_t>(end - begin));
  }
  return !out.empty();
}

bool ParsePointList(const char *text, std::vector<PerastageSvgPoint> &out) {
  std::vector<double> values;
  if (!ParseDoubles(text, values) || values.size() < 2)
    return false;
  out.clear();
  for (size_t i = 0; i + 1 < values.size(); i += 2)
    out.push_back({values[i], values[i + 1]});
  return !out.empty();
}

std::string TrimAscii(std::string value) {
  auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
  while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
    value.erase(value.begin());
  while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
    value.pop_back();
  return value;
}
std::optional<double> ParseSvgLengthToMillimeters(const char *text) {
  if (!text)
    return std::nullopt;

  std::string value = TrimAscii(text);
  if (value.empty())
    return std::nullopt;

  size_t unitPos = value.size();
  while (unitPos > 0) {
    const unsigned char ch = static_cast<unsigned char>(value[unitPos - 1]);
    if (!std::isalpha(ch))
      break;
    --unitPos;
  }

  std::string numberPart = TrimAscii(value.substr(0, unitPos));
  std::string unitPart = TrimAscii(value.substr(unitPos));
  std::transform(unitPart.begin(), unitPart.end(), unitPart.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

  char *end = nullptr;
  const double raw = std::strtod(numberPart.c_str(), &end);
  if (end == numberPart.c_str())
    return std::nullopt;

  if (unitPart.empty())
    return std::nullopt;
  if (unitPart == "mm")
    return raw;
  if (unitPart == "cm")
    return raw * 10.0;
  if (unitPart == "m")
    return raw * 1000.0;
  if (unitPart == "in")
    return raw * 25.4;
  if (unitPart == "pt")
    return raw * (25.4 / 72.0);
  if (unitPart == "pc")
    return raw * (25.4 / 6.0);
  if (unitPart == "px")
    return raw * (25.4 / 96.0);

  return std::nullopt;
}

void ScaleSvgGeometry(PerastageSvgSymbolData &symbol, double scaleX,
                      double scaleY) {
  for (auto &polygon : symbol.fills) {
    for (auto &point : polygon.points) {
      point.x *= scaleX;
      point.y *= scaleY;
    }
    for (auto &hole : polygon.holes) {
      for (auto &point : hole) {
        point.x *= scaleX;
        point.y *= scaleY;
      }
    }
  }

  for (auto &line : symbol.strokes) {
    for (auto &point : line.points) {
      point.x *= scaleX;
      point.y *= scaleY;
    }
  }

  symbol.viewBoxWidth *= scaleX;
  symbol.viewBoxHeight *= scaleY;
}


std::optional<double> ParsePercentOrInt255(std::string value) {
  value = TrimAscii(std::move(value));
  if (value.empty())
    return std::nullopt;
  if (value.back() == '%') {
    value.pop_back();
    char *end = nullptr;
    const double pct = std::strtod(value.c_str(), &end);
    if (end == value.c_str())
      return std::nullopt;
    return std::clamp(pct / 100.0, 0.0, 1.0);
  }
  char *end = nullptr;
  const double raw = std::strtod(value.c_str(), &end);
  if (end == value.c_str())
    return std::nullopt;
  return std::clamp(raw / 255.0, 0.0, 1.0);
}

bool ParseSvgFillColor(std::string value, double &r, double &g, double &b) {
  value = TrimAscii(std::move(value));
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  if (value.empty() || value == "none")
    return false;

  if (value == "white") {
    r = g = b = 1.0;
    return true;
  }

  if (value.size() == 7 && value[0] == '#') {
    const std::string rr = value.substr(1, 2);
    const std::string gg = value.substr(3, 2);
    const std::string bb = value.substr(5, 2);
    char *end = nullptr;
    const long rv = std::strtol(rr.c_str(), &end, 16);
    if (end == rr.c_str())
      return false;
    const long gv = std::strtol(gg.c_str(), &end, 16);
    if (end == gg.c_str())
      return false;
    const long bv = std::strtol(bb.c_str(), &end, 16);
    if (end == bb.c_str())
      return false;
    r = std::clamp(static_cast<double>(rv) / 255.0, 0.0, 1.0);
    g = std::clamp(static_cast<double>(gv) / 255.0, 0.0, 1.0);
    b = std::clamp(static_cast<double>(bv) / 255.0, 0.0, 1.0);
    return true;
  }

  if (value.size() == 4 && value[0] == '#') {
    auto parseHexNibble = [](char ch) -> int {
      if (ch >= '0' && ch <= '9')
        return ch - '0';
      if (ch >= 'a' && ch <= 'f')
        return 10 + (ch - 'a');
      return -1;
    };
    const int rn = parseHexNibble(value[1]);
    const int gn = parseHexNibble(value[2]);
    const int bn = parseHexNibble(value[3]);
    if (rn < 0 || gn < 0 || bn < 0)
      return false;
    r = (rn * 17) / 255.0;
    g = (gn * 17) / 255.0;
    b = (bn * 17) / 255.0;
    return true;
  }

  if (value.rfind("rgb(", 0) == 0 && value.back() == ')') {
    const std::string inner = value.substr(4, value.size() - 5);
    std::vector<std::string> channels;
    size_t start = 0;
    while (start < inner.size()) {
      size_t comma = inner.find(',', start);
      if (comma == std::string::npos)
        comma = inner.size();
      channels.push_back(inner.substr(start, comma - start));
      start = comma + 1;
    }
    if (channels.size() != 3)
      return false;
    auto rv = ParsePercentOrInt255(channels[0]);
    auto gv = ParsePercentOrInt255(channels[1]);
    auto bv = ParsePercentOrInt255(channels[2]);
    if (!rv.has_value() || !gv.has_value() || !bv.has_value())
      return false;
    r = rv.value();
    g = gv.value();
    b = bv.value();
    return true;
  }

  return false;
}

bool ExtractStyleFill(const char *styleAttr, std::string &outFill) {
  if (!styleAttr)
    return false;
  std::string style(styleAttr);
  size_t pos = 0;
  while (pos < style.size()) {
    size_t semi = style.find(';', pos);
    if (semi == std::string::npos)
      semi = style.size();
    std::string token = style.substr(pos, semi - pos);
    size_t colon = token.find(':');
    if (colon != std::string::npos) {
      std::string key = TrimAscii(token.substr(0, colon));
      std::transform(key.begin(), key.end(), key.begin(),
                     [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
      if (key == "fill") {
        outFill = TrimAscii(token.substr(colon + 1));
        return !outFill.empty();
      }
    }
    pos = semi + 1;
  }
  return false;
}

bool ElementForcesWhiteFill(const tinyxml2::XMLElement *element) {
  if (!element)
    return false;

  std::string fillText;
  if (const char *fillAttr = element->Attribute("fill"); fillAttr)
    fillText = fillAttr;
  if (fillText.empty())
    ExtractStyleFill(element->Attribute("style"), fillText);
  if (fillText.empty())
    return false;

  double r = 0.0;
  double g = 0.0;
  double b = 0.0;
  if (!ParseSvgFillColor(fillText, r, g, b))
    return false;
  constexpr double kWhiteThreshold = 0.98;
  return r >= kWhiteThreshold && g >= kWhiteThreshold && b >= kWhiteThreshold;
}

double SignedArea(const std::vector<PerastageSvgPoint> &polygon) {
  if (polygon.size() < 3)
    return 0.0;
  double area = 0.0;
  for (size_t i = 0; i < polygon.size(); ++i) {
    const auto &a = polygon[i];
    const auto &b = polygon[(i + 1) % polygon.size()];
    area += (a.x * b.y) - (b.x * a.y);
  }
  return area * 0.5;
}

bool IsPointOnSegment(const PerastageSvgPoint &point,
                      const PerastageSvgPoint &a,
                      const PerastageSvgPoint &b) {
  constexpr double kEpsilon = 1e-7;
  const double dx = b.x - a.x;
  const double dy = b.y - a.y;
  const double px = point.x - a.x;
  const double py = point.y - a.y;
  const double cross = dx * py - dy * px;
  if (std::abs(cross) > kEpsilon)
    return false;

  const double dot = px * dx + py * dy;
  if (dot < -kEpsilon)
    return false;

  const double lenSq = dx * dx + dy * dy;
  if (dot - lenSq > kEpsilon)
    return false;

  return true;
}

PerastageSvgPoint PolygonCentroid(
    const std::vector<PerastageSvgPoint> &polygon) {
  PerastageSvgPoint centroid{};
  if (polygon.empty())
    return centroid;

  for (const auto &point : polygon) {
    centroid.x += point.x;
    centroid.y += point.y;
  }
  const double invCount = 1.0 / static_cast<double>(polygon.size());
  centroid.x *= invCount;
  centroid.y *= invCount;
  return centroid;
}

bool IsPointInsidePolygon(const PerastageSvgPoint &point,
                          const std::vector<PerastageSvgPoint> &polygon) {
  bool inside = false;
  const size_t count = polygon.size();
  if (count < 3)
    return false;
  for (size_t i = 0, j = count - 1; i < count; j = i++) {
    const auto &pi = polygon[i];
    const auto &pj = polygon[j];

    if (IsPointOnSegment(point, pj, pi))
      return true;

    const bool intersects = ((pi.y > point.y) != (pj.y > point.y)) &&
                            (point.x < (pj.x - pi.x) * (point.y - pi.y) /
                                               ((pj.y - pi.y) == 0.0 ? 1e-12 : (pj.y - pi.y)) +
                                           pi.x);
    if (intersects)
      inside = !inside;
  }
  return inside;
}

void AssignWhitePolygonsAsHoles(
    const std::vector<std::pair<PerastageSvgPolygon, bool>> &rawPolygons,
    std::vector<PerastageSvgPolygon> &fills) {
  fills.clear();
  std::vector<double> fillAreas;
  for (const auto &entry : rawPolygons) {
    if (entry.second || entry.first.points.size() < 3)
      continue;
    fills.push_back(entry.first);
    fillAreas.push_back(std::abs(SignedArea(entry.first.points)));
  }

  for (const auto &entry : rawPolygons) {
    if (!entry.second || entry.first.points.size() < 3)
      continue;
    const PerastageSvgPoint anchor = PolygonCentroid(entry.first.points);
    size_t ownerIndex = fills.size();
    double ownerArea = 0.0;

    auto tryAssignOwner = [&](const PerastageSvgPoint &candidateAnchor) {
      for (size_t i = 0; i < fills.size(); ++i) {
        if (!IsPointInsidePolygon(candidateAnchor, fills[i].points))
          continue;
        if (ownerIndex == fills.size() || fillAreas[i] < ownerArea) {
          ownerIndex = i;
          ownerArea = fillAreas[i];
        }
      }
    };

    tryAssignOwner(anchor);
    if (ownerIndex == fills.size())
      tryAssignOwner(entry.first.points.front());
    if (ownerIndex < fills.size())
      fills[ownerIndex].holes.push_back(entry.first.points);
  }
}

// Reads only the writer's absolute, explicitly closed M/L/Z polygon subpaths.
// The first ring is the outer contour; subsequent rings are its real holes.
bool ParseCompoundPolygonPath(const char *data, PerastageSvgPolygon &polygon) {
  if (!data)
    return false;
  std::string_view remaining(data);
  auto skipSeparators = [&] {
    while (!remaining.empty() &&
           (remaining.front() == ',' ||
            std::isspace(static_cast<unsigned char>(remaining.front()))))
      remaining.remove_prefix(1);
  };
  auto readNumber = [&](double &value) {
    skipSeparators();
    const auto result = std::from_chars(
        remaining.data(), remaining.data() + remaining.size(), value);
    if (result.ec != std::errc{} || !std::isfinite(value))
      return false;
    remaining.remove_prefix(static_cast<size_t>(result.ptr - remaining.data()));
    return true;
  };
  PerastageSvgPolygon parsed;
  while (true) {
    skipSeparators();
    if (remaining.empty())
      break;
    if (remaining.front() != 'M')
      return false;
    remaining.remove_prefix(1);
    std::vector<PerastageSvgPoint> ring;
    while (true) {
      PerastageSvgPoint point;
      if (!readNumber(point.x) || !readNumber(point.y))
        return false;
      ring.push_back(point);
      skipSeparators();
      if (remaining.empty())
        return false;
      const char command = remaining.front();
      remaining.remove_prefix(1);
      if (command == 'Z')
        break;
      if (command != 'L')
        return false;
    }
    if (ring.size() < 3)
      return false;
    if (parsed.points.empty())
      parsed.points = std::move(ring);
    else
      parsed.holes.push_back(std::move(ring));
  }
  if (parsed.points.empty())
    return false;
  polygon = std::move(parsed);
  return true;
}

bool CollectSvgElements(
    const tinyxml2::XMLElement *node,
    std::vector<std::pair<PerastageSvgPolygon, bool>> &rawPolygons,
    std::vector<PerastageSvgPolyline> &strokes, bool strictCompoundPaths) {
  for (const tinyxml2::XMLElement *element = node ? node->FirstChildElement() : nullptr;
       element; element = element->NextSiblingElement()) {
    const std::string tag = element->Name() ? element->Name() : "";
    if (tag == "polygon") {
      std::vector<PerastageSvgPoint> polygonPoints;
      if (ParsePointList(element->Attribute("points"), polygonPoints) &&
          polygonPoints.size() >= 3) {
        rawPolygons.emplace_back(
            PerastageSvgPolygon{std::move(polygonPoints), {}},
            ElementForcesWhiteFill(element));
      }
    } else if (tag == "path" &&
               element->Attribute("fill-rule", "evenodd")) {
      PerastageSvgPolygon polygon;
      if (!ParseCompoundPolygonPath(element->Attribute("d"), polygon)) {
        if (strictCompoundPaths)
          return false;
      } else {
        rawPolygons.emplace_back(std::move(polygon), false);
      }
    } else if (tag == "polyline") {
      PerastageSvgPolyline line;
      if (ParsePointList(element->Attribute("points"), line.points) &&
          line.points.size() >= 2) {
        strokes.push_back(std::move(line));
      }
    }
    if (!CollectSvgElements(element, rawPolygons, strokes, strictCompoundPaths))
      return false;
  }
  return true;
}

struct SvgResourceMetadata {
  bool marked = false;
  bool declaredStandard = false;
  std::string version;
  double offsetXmm = 0.0;
  double offsetYmm = 0.0;
  bool offsetsUsable = true;
};

bool ParseSvgData(const std::string &svgXml, PerastageSvgSymbolData &out,
                  SvgResourceMetadata &metadata, bool strictCompoundPaths) {
  tinyxml2::XMLDocument doc;
  if (doc.Parse(svgXml.c_str(), svgXml.size()) != tinyxml2::XML_SUCCESS)
    return false;

  const tinyxml2::XMLElement *svg = doc.FirstChildElement("svg");
  if (!svg)
    return false;

  if (const char *version = svg->Attribute(kPerastageSymbolVersionAttribute)) {
    metadata.marked = true;
    metadata.version = version;
  }
  const char *set = svg->Attribute(kPerastageSymbolResourceSetAttribute);
  metadata.declaredStandard = metadata.marked && set &&
                             std::string_view(set) == kStandardGdtfSymbolResourceSetValue;
  for (const auto &[attribute, value] :
       {std::pair{kPerastageSymbolOffsetXAttribute, &metadata.offsetXmm},
        std::pair{kPerastageSymbolOffsetYAttribute, &metadata.offsetYmm}}) {
    if (svg->Attribute(attribute) &&
        svg->QueryDoubleAttribute(attribute, value) != tinyxml2::XML_SUCCESS)
      metadata.offsetsUsable = false;
  }

  std::vector<double> viewBox;
  if (!ParseDoubles(svg->Attribute("viewBox"), viewBox) || viewBox.size() < 4)
    return false;

  out.viewBoxWidth = viewBox[2];
  out.viewBoxHeight = viewBox[3];
  if (out.viewBoxWidth <= 0.0 || out.viewBoxHeight <= 0.0)
    return false;

  std::vector<std::pair<PerastageSvgPolygon, bool>> rawPolygons;
  out.strokes.clear();
  // Supported SVG geometry is independent from resource ownership/provenance.
  // Preserve authored import recovery for unsupported paths, while project and
  // positively identified generated resources must parse their complete subset.
  if (!CollectSvgElements(svg, rawPolygons, out.strokes,
                          strictCompoundPaths || metadata.marked))
    return false;
  AssignWhitePolygonsAsHoles(rawPolygons, out.fills);

  const std::optional<double> svgWidthMm =
      ParseSvgLengthToMillimeters(svg->Attribute("width"));
  const std::optional<double> svgHeightMm =
      ParseSvgLengthToMillimeters(svg->Attribute("height"));
  if (svgWidthMm.has_value() && svgHeightMm.has_value() &&
      *svgWidthMm > 0.0 && *svgHeightMm > 0.0) {
    const double scaleX = *svgWidthMm / viewBox[2];
    const double scaleY = *svgHeightMm / viewBox[3];
    if (std::isfinite(scaleX) && std::isfinite(scaleY) && scaleX > 0.0 &&
        scaleY > 0.0) {
      ScaleSvgGeometry(out, scaleX, scaleY);
    }
  }

  return out.IsValid();
}

bool ReadOffset(const tinyxml2::XMLElement *model, const char *attr,
                double &outValue) {
  if (!model || !attr)
    return false;
  float parsed = 0.0f;
  if (model->QueryFloatAttribute(attr, &parsed) != tinyxml2::XML_SUCCESS)
    return false;
  outValue = parsed;
  return true;
}

struct SymbolArchive {
  gdtf::ArchiveReadResult archive;
  tinyxml2::XMLDocument description;
  const tinyxml2::XMLElement *fixtureType = nullptr;
  const tinyxml2::XMLElement *model = nullptr;
};

std::string ArchiveDiagnostics(
    const std::vector<gdtf::ArchiveDiagnostic> &diagnostics) {
  std::string result;
  for (const auto &diagnostic : diagnostics) {
    if (!result.empty())
      result += " ";
    result += diagnostic.message;
  }
  return result;
}

// Uses the shared immutable GDTF archive reader and the existing model resolver.
bool ReadSymbolArchive(const std::string &path, SymbolArchive &symbolArchive,
                       bool requireModel, std::string &diagnostic) {
  symbolArchive.archive = gdtf::ReadGdtfArchive(PathUtils::PathFromUtf8(path));
  if (!symbolArchive.archive.Success()) {
    diagnostic = ArchiveDiagnostics(symbolArchive.archive.diagnostics);
    if (diagnostic.empty())
      diagnostic = "Could not read the GDTF archive.";
    return false;
  }
  const std::string &xml = symbolArchive.archive.descriptionXml;
  if (symbolArchive.description.Parse(xml.c_str(), xml.size()) !=
      tinyxml2::XML_SUCCESS) {
    diagnostic = "description.xml could not be parsed.";
    return false;
  }
  symbolArchive.fixtureType = ResolveFixtureType(symbolArchive.description);
  if (!symbolArchive.fixtureType) {
    diagnostic = "FixtureType is missing from description.xml.";
    return false;
  }
  symbolArchive.model = ResolveTargetModel(symbolArchive.fixtureType);
  if (requireModel && !symbolArchive.model) {
    diagnostic = "The fixture does not declare a usable Model.";
    return false;
  }
  return true;
}

std::vector<std::string> BuildViewResourcePaths(
    SymbolViewKind view, const std::vector<std::string> &baseNames) {
  std::vector<std::string> paths;
  for (const auto &name : baseNames) {
    switch (view) {
    case SymbolViewKind::Bottom:
      paths.push_back("models/svg/" + name + "_bottom.svg");
      paths.push_back("models/svg_bottom/" + name + ".svg");
      break;
    case SymbolViewKind::Front:
      paths.push_back("models/svg_front/" + name + ".svg");
      break;
    case SymbolViewKind::Left:
    case SymbolViewKind::Right:
      paths.push_back("models/svg_side/" + name + ".svg");
      break;
    default:
      paths.push_back("models/svg/" + name + ".svg");
      break;
    }
  }
  return paths;
}

// Matches only a view's own paths; filename-only reader recovery could otherwise
// mistake an authored Top entry for an absent Side or Front entry.
std::string ResolveStoredViewPath(const gdtf::ArchiveReadResult &archive,
                                 const std::string &requestedPath,
                                 bool &ambiguous) {
  std::string compatible;
  for (const auto &entry : archive.entries) {
    if (entry.directory)
      continue;
    const std::string path = NormalizeArchivePath(entry.path);
    if (path == requestedPath)
      return path;
    if (EqualsNoCase(path, requestedPath)) {
      if (!compatible.empty())
        ambiguous = true;
      compatible = path;
    }
  }
  return ambiguous ? std::string() : compatible;
}

void ReadViewOffsets(const tinyxml2::XMLElement *model, SymbolViewKind view,
                     PerastageSvgSymbolData &parsed) {
  if (view == SymbolViewKind::Front) {
    ReadOffset(model, "SVGFrontOffsetX", parsed.offsetXmm);
    ReadOffset(model, "SVGFrontOffsetY", parsed.offsetYmm);
  } else if (view == SymbolViewKind::Left || view == SymbolViewKind::Right) {
    ReadOffset(model, "SVGSideOffsetX", parsed.offsetXmm);
    ReadOffset(model, "SVGSideOffsetY", parsed.offsetYmm);
  } else {
    ReadOffset(model, "SVGOffsetX", parsed.offsetXmm);
    ReadOffset(model, "SVGOffsetY", parsed.offsetYmm);
  }
}

struct InspectedSvgResource {
  FixtureSymbolResource resource;
  PerastageSvgSymbolData data;
};

// Inspects one inventoried archive entry through the shared bounded reader.
InspectedSvgResource InspectResourcePath(const SymbolArchive &symbolArchive,
                                        SymbolViewKind view,
                                        const std::string &path,
                                        bool dedicated) {
  InspectedSvgResource inspected;
  auto &resource = inspected.resource;
  resource.viewKind = view;
  resource.archivePath = path;
  resource.standardGdtf = !dedicated && view != SymbolViewKind::Bottom;
  resource.resourceSet = resource.standardGdtf
                             ? FixtureSymbolResourceSet::StandardGdtf
                             : FixtureSymbolResourceSet::Perastage;
  bool ambiguous = false;
  const std::string storedPath =
      ResolveStoredViewPath(symbolArchive.archive, path, ambiguous);
  if (storedPath.empty() && !ambiguous)
    return inspected;

  resource.exists = true;
  resource.archivePath = storedPath.empty() ? path : storedPath;
  const bool legacy = !dedicated &&
                      HasLegacySymbolRevisionForView(symbolArchive.fixtureType, view);
  resource.provenance = dedicated ? FixtureSymbolProvenance::GeneratedPerastage
                                 : legacy ? FixtureSymbolProvenance::LegacyPerastage
                                          : FixtureSymbolProvenance::AuthoredGdtf;
  if (legacy) {
    resource.resourceSet = FixtureSymbolResourceSet::Perastage;
    resource.standardGdtf = false;
  }
  if (ambiguous) {
    resource.diagnostic = "SVG archive path has ambiguous case matches.";
    return inspected;
  }
  const auto payload = gdtf::ReadGdtfArchiveResource(
      symbolArchive.archive.sourcePath, storedPath);
  if (!payload.Success() || payload.filesystemFallback ||
      payload.entryPath != storedPath) {
    resource.diagnostic = ArchiveDiagnostics(payload.diagnostics);
    if (resource.diagnostic.empty())
      resource.diagnostic = "SVG archive entry is empty or unreadable.";
    return inspected;
  }

  auto &parsed = inspected.data;
  parsed.sourcePath = storedPath;
  parsed.viewKind = view;
  SvgResourceMetadata metadata;
  const std::string xml(payload.bytes.begin(), payload.bytes.end());
  resource.usable = ParseSvgData(xml, parsed, metadata, dedicated || legacy);
  if (!dedicated && view != SymbolViewKind::Bottom && metadata.declaredStandard) {
    // A future converter can explicitly identify standard output independently
    // from internal symbols or historical symbol-generation revisions.
    resource.resourceSet = FixtureSymbolResourceSet::StandardGdtf;
    resource.standardGdtf = true;
    resource.provenance = FixtureSymbolProvenance::GeneratedPerastage;
  } else if (!dedicated && metadata.marked) {
    resource.resourceSet = FixtureSymbolResourceSet::Perastage;
    resource.standardGdtf = false;
    resource.provenance = FixtureSymbolProvenance::LegacyPerastage;
  }
  parsed.provenance = resource.provenance;
  parsed.resourceSet = resource.resourceSet;
  if (!resource.usable) {
    resource.diagnostic = "SVG is malformed or contains no usable geometry.";
    return inspected;
  }
  if (dedicated) {
    parsed.offsetXmm = metadata.offsetXmm;
    parsed.offsetYmm = metadata.offsetYmm;
    if (!metadata.offsetsUsable || !std::isfinite(parsed.offsetXmm) ||
        !std::isfinite(parsed.offsetYmm)) {
      resource.usable = false;
      resource.diagnostic = "Perastage SVG offsets must be finite.";
      return inspected;
    }
    if (metadata.declaredStandard)
      resource.diagnostic = "The Perastage namespace cannot declare a standard GDTF resource.";
  } else {
    ReadViewOffsets(symbolArchive.model, view, parsed);
  }
  resource.offsetXmm = parsed.offsetXmm;
  resource.offsetYmm = parsed.offsetYmm;
  if (metadata.marked &&
      metadata.version != std::to_string(kCurrentPerastageSymbolResourceVersion)) {
    if (!resource.diagnostic.empty())
      resource.diagnostic += " ";
    resource.diagnostic += "Perastage SVG resource version '" + metadata.version +
                           "' is not supported; using compatible SVG geometry.";
  }
  if (storedPath != path) {
    if (!resource.diagnostic.empty())
      resource.diagnostic += " ";
    resource.diagnostic += "Using a case-insensitive archive path match.";
  }
  return inspected;
}

// Dedicated internal resources take precedence without hiding authored or
// legacy alternatives from inspection and consolidation.
std::vector<InspectedSvgResource> InspectViewCandidates(
    const SymbolArchive &symbolArchive, SymbolViewKind view,
    const std::vector<std::string> &baseNames) {
  std::vector<InspectedSvgResource> candidates;
  for (const auto &base : baseNames) {
    auto candidate = InspectResourcePath(
        symbolArchive, view, BuildPerastageFixtureSymbolPath(base, view), true);
    if (candidate.resource.exists)
      candidates.push_back(std::move(candidate));
  }
  for (const auto &path : BuildViewResourcePaths(view, baseNames)) {
    auto candidate = InspectResourcePath(symbolArchive, view, path, false);
    if (candidate.resource.exists)
      candidates.push_back(std::move(candidate));
  }
  return candidates;
}

} // namespace

bool InspectFixtureSymbolResources(
    const std::string &gdtfPath,
    FixtureSymbolResourceInspection &inspection) {
  inspection = {};
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  SymbolArchive symbolArchive;
  if (!ReadSymbolArchive(gdtfPath, symbolArchive, true, inspection.diagnostic))
    return false;
  inspection.modelSvgBasename = ResolveModelSvgBasename(symbolArchive.model);
  const auto baseNames = BuildSvgBaseNameCandidates(inspection.modelSvgBasename);
  for (auto &internal : inspection.perastageViews) {
    const auto view = internal.viewKind;
    internal.archivePath = BuildPerastageFixtureSymbolPath(inspection.modelSvgBasename, view);
    internal.diagnostic = "Perastage SVG archive entry is missing.";
    FixtureSymbolResource *standard = nullptr;
    for (auto &resource : inspection.standardViews) {
      if (resource.viewKind == view)
        standard = &resource;
    }
    if (standard) {
      standard->archivePath = BuildViewResourcePaths(view, baseNames).front();
      standard->diagnostic = "Standard GDTF SVG archive entry is missing.";
    }
    for (const auto &candidate : InspectViewCandidates(symbolArchive, view, baseNames)) {
      const auto &resource = candidate.resource;
      if (resource.resourceSet == FixtureSymbolResourceSet::Perastage) {
        inspection.perastageResources.push_back(resource);
        if (!internal.exists || (!internal.usable && resource.usable))
          internal = resource;
        if (standard && resource.provenance == FixtureSymbolProvenance::LegacyPerastage &&
            !standard->exists)
          standard->diagnostic = "The official SVG path contains a legacy Perastage symbol, not a standard resource.";
      } else {
        inspection.standardResources.push_back(resource);
        if (standard && (!standard->exists || (!standard->usable && resource.usable)))
          *standard = resource;
      }
    }
  }
  inspection.standardViewsUsable = std::all_of(
      inspection.standardViews.begin(), inspection.standardViews.end(),
      [](const auto &resource) { return resource.usable; });
  inspection.perastageViewsUsable = std::all_of(
      inspection.perastageViews.begin(), inspection.perastageViews.end(),
      [](const auto &resource) { return resource.usable; });
  if (!inspection.standardViewsUsable && !inspection.perastageViewsUsable) {
    inspection.diagnostic = "Neither the standard GDTF views nor the internal Perastage views are complete.";
    for (const auto &resource : inspection.standardViews) {
      if (!resource.usable) {
        inspection.diagnostic += " " + resource.archivePath + ": " + resource.diagnostic;
        break;
      }
    }
    for (const auto &resource : inspection.perastageViews) {
      if (!resource.usable) {
        inspection.diagnostic += " " + resource.archivePath + ": " + resource.diagnostic;
        break;
      }
    }
  }
  return true;
}

bool LoadPerastageSvgSymbolFromGdtf(const std::string &gdtfPath,
                                  SymbolViewKind requestedView,
                                  PerastageSvgSymbolData &out,
                                  std::string *errorDetails,
                                  FixtureSymbolResolutionPurpose purpose) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  out = {};
  FixtureSymbolResourceInspection inspection;
  InspectFixtureSymbolResources(gdtfPath, inspection);
  const auto resolved = ResolveFixtureSymbolView(inspection, requestedView, purpose);
  if (!resolved.usable) {
    if (errorDetails)
      *errorDetails = resolved.diagnostic;
    return false;
  }
  const auto payload = gdtf::ReadGdtfArchiveResource(
      PathUtils::PathFromUtf8(gdtfPath), resolved.archivePath);
  SvgResourceMetadata metadata;
  PerastageSvgSymbolData parsed;
  if (!payload.Success() || payload.filesystemFallback ||
      payload.entryPath != resolved.archivePath ||
      !ParseSvgData(std::string(payload.bytes.begin(), payload.bytes.end()), parsed,
                    metadata, resolved.provenance != FixtureSymbolProvenance::AuthoredGdtf)) {
    if (errorDetails)
      *errorDetails = "The resolved SVG resource could not be loaded: " + resolved.archivePath;
    return false;
  }
  parsed.sourcePath = resolved.archivePath;
  parsed.viewKind = requestedView == SymbolViewKind::Right &&
                            resolved.resolvedView == SymbolViewKind::Left
                        ? SymbolViewKind::Right : resolved.resolvedView;
  parsed.provenance = resolved.provenance;
  parsed.resourceSet = resolved.resourceSet;
  parsed.offsetXmm = resolved.offsetXmm;
  parsed.offsetYmm = resolved.offsetYmm;
  parsed.usedViewFallback = resolved.usedViewFallback;
  out = std::move(parsed);
  if (errorDetails)
    errorDetails->clear();
  return true;
}

bool ParsePerastageProjectSvgSymbol(const std::string &svg,
                                   PerastageSvgSymbolData &out,
                                   std::string *errorDetails) {
  return ParseFixtureSymbolSvg(svg, out, errorDetails);
}

bool ParseFixtureSymbolSvg(const std::string &svg, PerastageSvgSymbolData &out,
                           std::string *errorDetails) {
  out = {};
  SvgResourceMetadata metadata;
  const bool usable = ParseSvgData(svg, out, metadata, true);
  if (errorDetails)
    *errorDetails = usable ? "" : "The fixture symbol SVG is malformed or unsupported.";
  return usable;
}
