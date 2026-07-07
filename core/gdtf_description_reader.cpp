#include "gdtf_description_reader.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <set>

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

// Collects media/resource reference attributes from a wheel slot.
std::vector<std::string> CollectSlotResourceReferences(
    const tinyxml2::XMLElement *slot) {
  std::vector<std::string> refs;
  for (const char *name : {"MediaFileName", "Gobo", "File", "Image"}) {
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
    std::string normalized = ref;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    if (!archiveEntries.count(LowerAscii(normalized))) {
      AddDiagnostic(snapshot, DescriptionDiagnosticCode::MissingLocalResource,
                    "A GDTF resource reference does not match an archive entry.",
                    ref);
    }
  }
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
  for (std::string path : archiveEntryPaths) {
    std::replace(path.begin(), path.end(), '\\', '/');
    archiveEntries.insert(LowerAscii(path));
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
      snapshot.dmxModeNames.push_back(FirstAttribute(mode, {"Name"}));
    }
  }

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
        slotInfo.resourceReferences = CollectSlotResourceReferences(slot);
        ValidateLocalReferences(snapshot, slotInfo.resourceReferences,
                                archiveEntries);
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
