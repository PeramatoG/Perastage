#include "symbols/PerastageSvgSymbol.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <tinyxml2.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {
std::string NormalizeArchivePath(std::string value) {
  std::replace(value.begin(), value.end(), '\\', '/');
  while (!value.empty() && value.front() == '/')
    value.erase(value.begin());
  return value;
}

bool ReadAllBytes(wxZipInputStream &zip, std::string &out) {
  out.clear();
  char buffer[4096];
  while (true) {
    zip.Read(buffer, sizeof(buffer));
    const size_t count = zip.LastRead();
    if (count == 0)
      break;
    out.append(buffer, count);
  }
  return true;
}

bool ReadZipEntries(const std::string &zipPath,
                    std::unordered_map<std::string, std::string> &entries) {
  wxFileInputStream input(zipPath);
  if (!input.IsOk())
    return false;

  wxZipInputStream zipInput(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    std::string content;
    if (!ReadAllBytes(zipInput, content))
      continue;
    entries[NormalizeArchivePath(entry->GetName().ToStdString())] =
        std::move(content);
  }
  return true;
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

bool IsPointInsidePolygon(const PerastageSvgPoint &point,
                          const std::vector<PerastageSvgPoint> &polygon) {
  bool inside = false;
  const size_t count = polygon.size();
  if (count < 3)
    return false;
  for (size_t i = 0, j = count - 1; i < count; j = i++) {
    const auto &pi = polygon[i];
    const auto &pj = polygon[j];
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
    const std::vector<std::pair<std::vector<PerastageSvgPoint>, bool>> &rawPolygons,
    std::vector<PerastageSvgPolygon> &fills) {
  fills.clear();
  std::vector<double> fillAreas;
  for (const auto &entry : rawPolygons) {
    if (entry.second || entry.first.size() < 3)
      continue;
    PerastageSvgPolygon fill{};
    fill.points = entry.first;
    fills.push_back(std::move(fill));
    fillAreas.push_back(std::abs(SignedArea(entry.first)));
  }

  for (const auto &entry : rawPolygons) {
    if (!entry.second || entry.first.size() < 3)
      continue;
    const PerastageSvgPoint anchor = entry.first.front();
    size_t ownerIndex = fills.size();
    double ownerArea = 0.0;
    for (size_t i = 0; i < fills.size(); ++i) {
      if (!IsPointInsidePolygon(anchor, fills[i].points))
        continue;
      if (ownerIndex == fills.size() || fillAreas[i] < ownerArea) {
        ownerIndex = i;
        ownerArea = fillAreas[i];
      }
    }
    if (ownerIndex < fills.size())
      fills[ownerIndex].holes.push_back(entry.first);
  }
}

void CollectSvgElements(const tinyxml2::XMLElement *node,
                        std::vector<std::pair<std::vector<PerastageSvgPoint>, bool>> &rawPolygons,
                        std::vector<PerastageSvgPolyline> &strokes) {
  for (const tinyxml2::XMLElement *element = node ? node->FirstChildElement() : nullptr;
       element; element = element->NextSiblingElement()) {
    const std::string tag = element->Name() ? element->Name() : "";
    if (tag == "polygon") {
      std::vector<PerastageSvgPoint> polygonPoints;
      if (ParsePointList(element->Attribute("points"), polygonPoints) &&
          polygonPoints.size() >= 3) {
        rawPolygons.emplace_back(std::move(polygonPoints),
                                 ElementForcesWhiteFill(element));
      }
    } else if (tag == "polyline") {
      PerastageSvgPolyline line;
      if (ParsePointList(element->Attribute("points"), line.points) &&
          line.points.size() >= 2) {
        strokes.push_back(std::move(line));
      }
    }
    CollectSvgElements(element, rawPolygons, strokes);
  }
}

bool ParseSvgData(const std::string &svgXml, PerastageSvgSymbolData &out) {
  tinyxml2::XMLDocument doc;
  if (doc.Parse(svgXml.c_str(), svgXml.size()) != tinyxml2::XML_SUCCESS)
    return false;

  const tinyxml2::XMLElement *svg = doc.FirstChildElement("svg");
  if (!svg)
    return false;

  std::vector<double> viewBox;
  if (!ParseDoubles(svg->Attribute("viewBox"), viewBox) || viewBox.size() < 4)
    return false;

  out.viewBoxWidth = viewBox[2];
  out.viewBoxHeight = viewBox[3];
  if (out.viewBoxWidth <= 0.0 || out.viewBoxHeight <= 0.0)
    return false;

  std::vector<std::pair<std::vector<PerastageSvgPoint>, bool>> rawPolygons;
  out.strokes.clear();
  CollectSvgElements(svg, rawPolygons, out.strokes);
  AssignWhitePolygonsAsHoles(rawPolygons, out.fills);
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
} // namespace

bool LoadPerastageSvgSymbolFromGdtf(const std::string &gdtfPath,
                                    SymbolViewKind requestedView,
                                    PerastageSvgSymbolData &out) {
  std::unordered_map<std::string, std::string> entries;
  if (!ReadZipEntries(gdtfPath, entries))
    return false;

  auto descIt = entries.find("description.xml");
  if (descIt == entries.end())
    return false;

  tinyxml2::XMLDocument description;
  if (description.Parse(descIt->second.c_str(), descIt->second.size()) !=
      tinyxml2::XML_SUCCESS) {
    return false;
  }

  const tinyxml2::XMLElement *fixtureType = ResolveFixtureType(description);
  if (!fixtureType)
    return false;

  const char *editor = fixtureType->Attribute("Editor");
  if (!editor || !EqualsNoCase(editor, "Perastage"))
    return false;

  const tinyxml2::XMLElement *model = ResolveTargetModel(fixtureType);
  const std::string baseName = ResolveModelSvgBasename(model);

  struct Candidate {
    SymbolViewKind viewKind;
    std::string archivePath;
    const char *offsetXAttr;
    const char *offsetYAttr;
  };

  std::vector<Candidate> candidates;
  if (requestedView == SymbolViewKind::Front) {
    candidates.push_back({SymbolViewKind::Front,
                          "models/svg_front/" + baseName + ".svg",
                          "SVGFrontOffsetX", "SVGFrontOffsetY"});
    candidates.push_back({SymbolViewKind::Top, "models/svg/" + baseName + ".svg",
                          "SVGOffsetX", "SVGOffsetY"});
  } else {
    candidates.push_back({SymbolViewKind::Top, "models/svg/" + baseName + ".svg",
                          "SVGOffsetX", "SVGOffsetY"});
  }

  for (const auto &candidate : candidates) {
    auto svgIt = entries.find(candidate.archivePath);
    if (svgIt == entries.end())
      continue;

    PerastageSvgSymbolData parsed;
    parsed.sourcePath = candidate.archivePath;
    parsed.viewKind = candidate.viewKind;
    ReadOffset(model, candidate.offsetXAttr, parsed.offsetXmm);
    ReadOffset(model, candidate.offsetYAttr, parsed.offsetYmm);
    if (!ParseSvgData(svgIt->second, parsed))
      continue;

    out = std::move(parsed);
    return true;
  }

  return false;
}
