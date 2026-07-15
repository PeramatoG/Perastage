#include "gdtf_description_reader.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <initializer_list>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <set>
#include <vector>

#include <tinyxml2.h>

namespace gdtf {
namespace {
// Returns the first non-empty attribute from an XML element.
std::string FirstAttribute(const tinyxml2::XMLElement *element,
                           std::initializer_list<const char *> names) {
  if (!element)
    return {};
  for (const char *name : names) {
    if (const char *value = element->Attribute(name); value && *value)
      return value;
  }
  return {};
}

// Converts ASCII characters to lower case for reference lookup.
std::string LowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

// Normalizes archive separators without changing case or resource text.
std::string NormalizeArchiveSeparators(std::string value) {
  std::replace(value.begin(), value.end(), '\\', '/');
  while (value.rfind("./", 0) == 0)
    value.erase(0, 2);
  return value;
}

// Parses a float attribute from a small set of accepted GDTF attribute names.
bool ParseFloatAttribute(const tinyxml2::XMLElement *element,
                         std::initializer_list<const char *> names,
                         float &outValue) {
  if (!element)
    return false;
  for (const char *name : names) {
    if (!name)
      continue;
    if (const char *raw = element->Attribute(name)) {
      char *end = nullptr;
      const float parsed = std::strtof(raw, &end);
      if (end && end != raw && *end == '\0') {
        outValue = parsed;
        return true;
      }
    }
  }
  return false;
}

// Adds positive values only when an attribute can be parsed as a number.
bool AddPositiveFloatAttribute(const tinyxml2::XMLElement *element,
                               std::initializer_list<const char *> names,
                               float &total) {
  float parsed = 0.0f;
  if (!ParseFloatAttribute(element, names, parsed) || parsed <= 0.0f)
    return false;
  total += parsed;
  return true;
}

// Recursively sums consumer wiring-object payloads from a geometry subtree.
float SumConsumerWiringPower(const tinyxml2::XMLElement *element) {
  float total = 0.0f;
  for (const tinyxml2::XMLElement *child =
           element ? element->FirstChildElement() : nullptr;
       child; child = child->NextSiblingElement()) {
    const std::string name = child->Name() ? child->Name() : "";
    if (name == "WiringObject") {
      const char *componentType = child->Attribute("ComponentType");
      if (componentType && std::string(componentType) == "Consumer") {
        AddPositiveFloatAttribute(child, {"ElectricalPayLoad",
                                          "ElectricalPayload"},
                                  total);
      }
    }
    total += SumConsumerWiringPower(child);
  }
  return total;
}

// Converts GDTF xyY color text to a display hex RGB value when possible.
std::string ParseModelColorHex(const tinyxml2::XMLElement *fixtureType) {
  const tinyxml2::XMLElement *models =
      fixtureType ? fixtureType->FirstChildElement("Models") : nullptr;
  const tinyxml2::XMLElement *model =
      models ? models->FirstChildElement("Model") : nullptr;
  const char *raw = model ? model->Attribute("Color") : nullptr;
  if (!raw)
    return {};

  std::string text = raw;
  std::replace(text.begin(), text.end(), ',', ' ');
  std::stringstream input(text);
  double x = 0.0;
  double y = 0.0;
  double luminance = 0.0;
  if (!(input >> x >> y >> luminance) || y <= 0.0)
    return {};

  const double X = x * (luminance / y);
  const double Z = (1.0 - x - y) * (luminance / y);
  auto gamma = [](double c) {
    c = std::max(0.0, c);
    return c <= 0.0031308 ? 12.92 * c
                           : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
  };
  const int red = static_cast<int>(
      std::round(std::clamp(gamma(3.2406 * X - 1.5372 * luminance - 0.4986 * Z),
                            0.0, 1.0) *
                 255.0));
  const int green = static_cast<int>(
      std::round(std::clamp(gamma(-0.9689 * X + 1.8758 * luminance + 0.0415 * Z),
                            0.0, 1.0) *
                 255.0));
  const int blue = static_cast<int>(
      std::round(std::clamp(gamma(0.0557 * X - 0.2040 * luminance + 1.0570 * Z),
                            0.0, 1.0) *
                 255.0));
  std::ostringstream output;
  output << '#' << std::uppercase << std::hex << std::setfill('0')
         << std::setw(2) << red << std::setw(2) << green << std::setw(2)
         << blue;
  return output.str();
}

// Adds a structured diagnostic to the description snapshot.
void AddDiagnostic(GdtfDescriptionSnapshot &snapshot,
                   DescriptionDiagnosticCode code, std::string message,
                   std::string path = {}) {
  snapshot.diagnostics.push_back({code, std::move(message), std::move(path)});
}

// Records unknown direct child elements without treating them as fatal.
void RecordUnknownChildren(GdtfDescriptionSnapshot &snapshot,
                           const tinyxml2::XMLElement *parent,
                           const std::set<std::string> &knownNames,
                           const std::string &parentPath) {
  for (const tinyxml2::XMLElement *child = parent ? parent->FirstChildElement()
                                                   : nullptr;
       child; child = child->NextSiblingElement()) {
    const std::string name = child->Name() ? child->Name() : "";
    if (!knownNames.count(name)) {
      AddDiagnostic(snapshot, DescriptionDiagnosticCode::UnknownElement,
                    "Unknown GDTF element was preserved as unread semantic data.",
                    parentPath + "/" + name);
    }
  }
}

// Collects generic resource reference attributes from a wheel slot.
std::vector<std::string> CollectSlotResourceReferences(
    const tinyxml2::XMLElement *slot) {
  std::vector<std::string> refs;
  for (const char *name : {"Gobo", "File", "Image"}) {
    if (const char *value = slot->Attribute(name); value && *value)
      refs.emplace_back(value);
  }
  return refs;
}

// Checks references that can be validated against archive entry names.
void ValidateLocalReferences(GdtfDescriptionSnapshot &snapshot,
                             const std::vector<std::string> &refs,
                             const std::set<std::string> &archiveEntries) {
  if (archiveEntries.empty())
    return;
  for (const std::string &ref : refs) {
    if (ref.empty())
      continue;
    std::string normalized = NormalizeArchiveSeparators(ref);
    if (!archiveEntries.count(LowerAscii(normalized))) {
      AddDiagnostic(snapshot, DescriptionDiagnosticCode::MissingLocalResource,
                    "A GDTF resource reference does not match an archive entry.",
                    ref);
    }
  }
}

// Resolves a wheel slot MediaFileName against canonical wheels resources.
void ValidateWheelMediaReference(GdtfDescriptionSnapshot &snapshot,
                                 const std::string &mediaFileName,
                                 const std::set<std::string> &archiveEntries,
                                 const std::set<std::string> &lowerArchiveEntries) {
  if (mediaFileName.empty() || archiveEntries.empty())
    return;

  const std::string base = NormalizeArchiveSeparators(mediaFileName);
  const std::vector<std::string> canonicalCandidates = {
      "wheels/" + base + ".png", "wheels/" + base + ".svg"};
  std::vector<std::string> exactMatches;
  for (const std::string &candidate : canonicalCandidates) {
    if (archiveEntries.count(candidate))
      exactMatches.push_back(candidate);
  }
  if (exactMatches.size() == 1)
    return;
  if (exactMatches.size() > 1) {
    AddDiagnostic(snapshot,
                  DescriptionDiagnosticCode::AmbiguousWheelMediaResource,
                  "A GDTF wheel media reference matches multiple canonical resources.",
                  mediaFileName);
    return;
  }

  std::vector<std::string> caseMatches;
  for (const std::string &candidate : canonicalCandidates) {
    if (lowerArchiveEntries.count(LowerAscii(candidate)))
      caseMatches.push_back(candidate);
  }
  if (caseMatches.size() == 1) {
    AddDiagnostic(snapshot,
                  DescriptionDiagnosticCode::NonCanonicalWheelMediaCaseMatch,
                  "A GDTF wheel media resource was accepted through a case-insensitive compatibility match.",
                  mediaFileName);
    return;
  }
  if (caseMatches.size() > 1) {
    AddDiagnostic(snapshot,
                  DescriptionDiagnosticCode::AmbiguousWheelMediaResource,
                  "A GDTF wheel media reference has multiple case-insensitive resource matches.",
                  mediaFileName);
    return;
  }

  AddDiagnostic(snapshot, DescriptionDiagnosticCode::MissingWheelMediaResource,
                "A GDTF wheel media reference does not match a wheels resource.",
                mediaFileName);
}
} // namespace

// Reports whether the description contains a readable GDTF FixtureType.
bool GdtfDescriptionSnapshot::Success() const {
  for (const DescriptionDiagnostic &diagnostic : diagnostics) {
    if (diagnostic.code == DescriptionDiagnosticCode::MalformedXml ||
        diagnostic.code == DescriptionDiagnosticCode::MissingRoot ||
        diagnostic.code == DescriptionDiagnosticCode::MissingFixtureType) {
      return false;
    }
  }
  return true;
}

// Parses a small read-only semantic snapshot from description.xml.
GdtfDescriptionSnapshot ReadGdtfDescription(
    const std::string &descriptionXml,
    const std::vector<std::string> &archiveEntryPaths) {
  GdtfDescriptionSnapshot snapshot;
  tinyxml2::XMLDocument doc;
  if (doc.Parse(descriptionXml.data(), descriptionXml.size()) !=
      tinyxml2::XML_SUCCESS) {
    AddDiagnostic(snapshot, DescriptionDiagnosticCode::MalformedXml,
                  "description.xml is not well-formed XML.");
    return snapshot;
  }

  const tinyxml2::XMLElement *root = doc.FirstChildElement("GDTF");
  if (!root) {
    AddDiagnostic(snapshot, DescriptionDiagnosticCode::MissingRoot,
                  "description.xml does not contain a GDTF root element.");
    return snapshot;
  }
  snapshot.dataVersion = FirstAttribute(root, {"DataVersion", "Version"});

  const tinyxml2::XMLElement *fixtureType = root->FirstChildElement("FixtureType");
  if (!fixtureType) {
    AddDiagnostic(snapshot, DescriptionDiagnosticCode::MissingFixtureType,
                  "description.xml does not contain a FixtureType element.");
    return snapshot;
  }

  snapshot.fixtureTypeName = FirstAttribute(fixtureType, {"Name"});
  snapshot.manufacturer = FirstAttribute(fixtureType, {"Manufacturer"});
  snapshot.shortName = FirstAttribute(fixtureType, {"ShortName"});
  snapshot.longName = FirstAttribute(fixtureType, {"LongName"});
  snapshot.description = FirstAttribute(fixtureType, {"Description"});
  snapshot.fixtureTypeId = FirstAttribute(fixtureType, {"FixtureTypeID"});
  snapshot.thumbnail = FirstAttribute(fixtureType, {"Thumbnail"});
  snapshot.createDate = FirstAttribute(
      fixtureType, {"CreateDate", "CreationDate", "DateCreated"});
  snapshot.revision =
      FirstAttribute(fixtureType, {"Revision", "DataVersion", "Version"});

  std::set<std::string> archiveEntries;
  std::set<std::string> lowerArchiveEntries;
  for (std::string path : archiveEntryPaths) {
    path = NormalizeArchiveSeparators(path);
    archiveEntries.insert(path);
    lowerArchiveEntries.insert(LowerAscii(path));
  }

  if (const tinyxml2::XMLElement *revisions =
          fixtureType->FirstChildElement("Revisions")) {
    for (const tinyxml2::XMLElement *rev = revisions->FirstChildElement("Revision");
         rev; rev = rev->NextSiblingElement("Revision")) {
      snapshot.revisions.push_back({FirstAttribute(rev, {"Text", "Comment", "Version"}),
                                    FirstAttribute(rev, {"Date", "TimeStamp"}),
                                    FirstAttribute(rev, {"UserID"}),
                                    FirstAttribute(rev, {"ModifiedBy"})});
    }
  }

  if (const tinyxml2::XMLElement *dmxModes =
          fixtureType->FirstChildElement("DMXModes")) {
    for (const tinyxml2::XMLElement *mode = dmxModes->FirstChildElement("DMXMode");
         mode; mode = mode->NextSiblingElement("DMXMode")) {
      if (std::string modeName = FirstAttribute(mode, {"Name"});
          !modeName.empty())
        snapshot.dmxModeNames.push_back(std::move(modeName));
    }
  } else {
    AddDiagnostic(snapshot, DescriptionDiagnosticCode::MissingDmxModes,
                  "description.xml does not contain a DMXModes element.");
  }
  if (snapshot.dmxModeNames.empty()) {
    AddDiagnostic(snapshot, DescriptionDiagnosticCode::MissingUsableDmxMode,
                  "description.xml does not contain a usable named DMXMode.");
  }

  if (const tinyxml2::XMLElement *physicalDescriptions =
          fixtureType->FirstChildElement("PhysicalDescriptions")) {
    if (const tinyxml2::XMLElement *properties =
            physicalDescriptions->FirstChildElement("Properties")) {
      if (const tinyxml2::XMLElement *weight =
              properties->FirstChildElement("Weight")) {
        snapshot.weightKgPresent =
            ParseFloatAttribute(weight, {"Value", "value"}, snapshot.weightKg);
      }
      float explicitPower = 0.0f;
      for (const tinyxml2::XMLElement *power =
               properties->FirstChildElement("PowerConsumption");
           power; power = power->NextSiblingElement("PowerConsumption")) {
        AddPositiveFloatAttribute(power, {"Value", "PowerConsumption", "value"},
                                  explicitPower);
      }
      if (explicitPower > 0.0f) {
        snapshot.powerConsumptionW = explicitPower;
        snapshot.powerConsumptionWPresent = true;
      }
    }
  }
  if (const tinyxml2::XMLElement *geometries =
          fixtureType->FirstChildElement("Geometries")) {
    if (const tinyxml2::XMLElement *structure =
            geometries->FirstChildElement("Structure")) {
      snapshot.trussCrossSectionType =
          FirstAttribute(structure, {"CrossSectionType"});
      snapshot.trussCrossSection = FirstAttribute(structure, {"TrussCrossSection"});
    }
  }

  if (!snapshot.powerConsumptionWPresent) {
    const tinyxml2::XMLElement *geometries =
        fixtureType->FirstChildElement("Geometries");
    const float wiringPower = SumConsumerWiringPower(geometries);
    if (wiringPower > 0.0f) {
      snapshot.powerConsumptionW = wiringPower;
      snapshot.powerConsumptionWPresent = true;
    }
  }
  snapshot.modelColorHex = ParseModelColorHex(fixtureType);

  if (const tinyxml2::XMLElement *wheels = fixtureType->FirstChildElement("Wheels")) {
    for (const tinyxml2::XMLElement *wheel = wheels->FirstChildElement("Wheel");
         wheel; wheel = wheel->NextSiblingElement("Wheel")) {
      GdtfWheelInfo wheelInfo;
      wheelInfo.name = FirstAttribute(wheel, {"Name"});
      for (const tinyxml2::XMLElement *slot = wheel->FirstChildElement("Slot");
           slot; slot = slot->NextSiblingElement("Slot")) {
        GdtfWheelSlotInfo slotInfo;
        slotInfo.name = FirstAttribute(slot, {"Name"});
        slotInfo.mediaFileName = FirstAttribute(slot, {"MediaFileName"});
        ValidateWheelMediaReference(snapshot, slotInfo.mediaFileName,
                                    archiveEntries, lowerArchiveEntries);
        slotInfo.resourceReferences = CollectSlotResourceReferences(slot);
        ValidateLocalReferences(snapshot, slotInfo.resourceReferences,
                                lowerArchiveEntries);
        wheelInfo.slots.push_back(std::move(slotInfo));
      }
      snapshot.wheels.push_back(std::move(wheelInfo));
    }
  }

  RecordUnknownChildren(snapshot, root, {"FixtureType"}, "/GDTF");
  RecordUnknownChildren(snapshot, fixtureType,
                        {"Revisions", "AttributeDefinitions", "Wheels",
                         "PhysicalDescriptions", "Models", "Geometries",
                         "DMXModes", "Protocols"},
                        "/GDTF/FixtureType");
  return snapshot;
}

} // namespace gdtf
