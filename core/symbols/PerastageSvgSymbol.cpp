#include "symbols/PerastageSvgSymbol.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <unordered_map>

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

void CollectSvgElements(const tinyxml2::XMLElement *node,
                        std::vector<PerastageSvgPolygon> &fills,
                        std::vector<PerastageSvgPolyline> &strokes) {
  for (const tinyxml2::XMLElement *element = node ? node->FirstChildElement() : nullptr;
       element; element = element->NextSiblingElement()) {
    const std::string tag = element->Name() ? element->Name() : "";
    if (tag == "polygon") {
      PerastageSvgPolygon polygon;
      if (ParsePointList(element->Attribute("points"), polygon.points) &&
          polygon.points.size() >= 3) {
        fills.push_back(std::move(polygon));
      }
    } else if (tag == "polyline") {
      PerastageSvgPolyline line;
      if (ParsePointList(element->Attribute("points"), line.points) &&
          line.points.size() >= 2) {
        strokes.push_back(std::move(line));
      }
    }
    CollectSvgElements(element, fills, strokes);
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

  out.fills.clear();
  out.strokes.clear();
  CollectSvgElements(svg, out.fills, out.strokes);
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
