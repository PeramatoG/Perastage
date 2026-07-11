#include "gdtf/gdtf_wheel_catalog.h"

#include <algorithm>
#include <cctype>

#include <tinyxml2.h>

namespace gdtf {
namespace {
// Reads an XML attribute without changing omitted values.
std::string Attr(const tinyxml2::XMLElement *element, const char *name) {
  if (!element)
    return {};
  if (const char *value = element->Attribute(name))
    return value;
  return {};
}

// Converts text to lowercase for classification checks.
std::string LowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

// Adds a catalog diagnostic.
void AddDiagnostic(std::vector<GdtfModeDiagnostic> &diagnostics,
                   GdtfDiagnosticSeverity severity, std::string message,
                   std::string context = {}, std::string rawValue = {},
                   std::string nodeId = {}) {
  diagnostics.push_back({severity, std::move(message), std::move(context),
                         std::move(rawValue), std::move(nodeId)});
}

// Builds a deterministic catalog child id.
std::string ChildId(const std::string &parent, const char *kind, int index) {
  return parent + "/" + kind + "[" + std::to_string(index) + "]";
}

// Reports whether a wheel or slot exposes graphic-wheel metadata.
bool IsGraphicWheelText(const std::string &text) {
  return LowerAscii(text).find("graphic") != std::string::npos;
}

// Reads child element names as preserved metadata labels.
std::vector<std::string> ReadChildNames(const tinyxml2::XMLElement *parent, const char *name) {
  std::vector<std::string> result;
  for (const auto *child = parent ? parent->FirstChildElement(name) : nullptr; child;
       child = child->NextSiblingElement(name)) {
    const std::string childName = Attr(child, "Name");
    result.push_back(childName.empty() ? name : childName);
  }
  return result;
}

// Finds a direct child element by name below a fixture type.
const tinyxml2::XMLElement *FindSection(const tinyxml2::XMLElement *fixtureType,
                                        const char *name) {
  return fixtureType ? fixtureType->FirstChildElement(name) : nullptr;
}
} // namespace

// Finds a wheel by exact GDTF name.
const GdtfWheelInfo *GdtfWheelCatalog::FindWheel(const std::string &name) const {
  for (const auto &wheel : wheels) {
    if (wheel.name == name)
      return &wheel;
  }
  return nullptr;
}

// Finds a filter by exact GDTF name.
const GdtfFilterInfo *GdtfWheelCatalog::FindFilter(const std::string &name) const {
  for (const auto &filter : filters) {
    if (filter.name == name)
      return &filter;
  }
  return nullptr;
}

// Reads GDTF wheels, slots, filters, and graphic-wheel metadata from description.xml.
GdtfWheelCatalog ReadGdtfWheelCatalog(const std::string &descriptionXml) {
  GdtfWheelCatalog catalog;
  tinyxml2::XMLDocument xml;
  if (xml.Parse(descriptionXml.c_str(), descriptionXml.size()) != tinyxml2::XML_SUCCESS) {
    AddDiagnostic(catalog.diagnostics, GdtfDiagnosticSeverity::Error,
                  "Malformed GDTF description XML.");
    return catalog;
  }
  const auto *root = xml.FirstChildElement("GDTF");
  const auto *fixtureType = root ? root->FirstChildElement("FixtureType") : nullptr;
  const auto *filters = FindSection(fixtureType, "Filters");
  int filterIndex = 0;
  for (const auto *filterXml = filters ? filters->FirstChildElement("Filter") : nullptr;
       filterXml; filterXml = filterXml->NextSiblingElement("Filter"), ++filterIndex) {
    GdtfFilterInfo filter;
    filter.sourceIndex = filterIndex;
    filter.id = "filter[" + std::to_string(filterIndex) + "]:" + Attr(filterXml, "Name");
    filter.name = Attr(filterXml, "Name");
    filter.rawColor = Attr(filterXml, "Color");
    filter.color = ParseGdtfColorCie(filter.rawColor, filter.rawColor.empty()
                                                     ? GdtfValueOrigin::Unavailable
                                                     : GdtfValueOrigin::Explicit);
    catalog.filters.push_back(std::move(filter));
  }

  const auto *wheels = FindSection(fixtureType, "Wheels");
  int wheelIndex = 0;
  for (const auto *wheelXml = wheels ? wheels->FirstChildElement("Wheel") : nullptr;
       wheelXml; wheelXml = wheelXml->NextSiblingElement("Wheel"), ++wheelIndex) {
    GdtfWheelInfo wheel;
    wheel.sourceIndex = wheelIndex;
    wheel.name = Attr(wheelXml, "Name");
    wheel.type = Attr(wheelXml, "Type");
    wheel.id = "wheel[" + std::to_string(wheelIndex) + "]:" + wheel.name;
    wheel.graphicWheel = IsGraphicWheelText(wheel.type) || IsGraphicWheelText(wheel.name);
    int slotIndex = 0;
    for (const auto *slotXml = wheelXml->FirstChildElement("WheelSlot"); slotXml;
         slotXml = slotXml->NextSiblingElement("WheelSlot"), ++slotIndex) {
      GdtfWheelSlotInfo slot;
      slot.index = slotIndex + 1;
      slot.id = ChildId(wheel.id, "slot", slotIndex);
      slot.name = Attr(slotXml, "Name");
      slot.rawColor = Attr(slotXml, "Color");
      slot.color = ParseGdtfColorCie(slot.rawColor, slot.rawColor.empty()
                                                    ? GdtfValueOrigin::Unavailable
                                                    : GdtfValueOrigin::Explicit);
      slot.rawFilter = Attr(slotXml, "Filter");
      slot.mediaFileName = Attr(slotXml, "MediaFileName");
      slot.resolvedResourcePath = slot.mediaFileName;
      slot.prismFacets = ReadChildNames(slotXml, "PrismFacet");
      slot.animationSystems = ReadChildNames(slotXml, "AnimationSystem");
      slot.graphicWheelReference = Attr(slotXml, "GraphicWheel");
      slot.graphicWheelResource = Attr(slotXml, "GraphicWheelMediaFileName");
      if (slot.graphicWheelResource.empty())
        slot.graphicWheelResource = Attr(slotXml, "GraphicWheelResource");
      if (!slot.graphicWheelReference.empty() || !slot.graphicWheelResource.empty())
        wheel.graphicWheel = true;
      if (!slot.rawFilter.empty() && !catalog.FindFilter(slot.rawFilter))
        AddDiagnostic(slot.diagnostics, GdtfDiagnosticSeverity::Warning,
                      "WheelSlot filter reference could not be resolved exactly.",
                      "WheelSlot", slot.rawFilter, slot.id);
      wheel.slots.push_back(std::move(slot));
    }
    catalog.wheels.push_back(std::move(wheel));
  }
  return catalog;
}

} // namespace gdtf
