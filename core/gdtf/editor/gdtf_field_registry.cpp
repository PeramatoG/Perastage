#include "gdtf_field_registry.h"

#include <array>

namespace gdtf {
namespace {

constexpr std::array<GdtfFieldDescriptor, 31> kDescriptors = {{
    {GdtfFieldId::FixtureId, GdtfFieldOwnership::MvrProjectInstanceLevel,
     GdtfFieldValueKind::HostProjectValue, "fixture_id", "Fixture ID", true,
     false, false, false},
    {GdtfFieldId::FixtureInstanceName,
     GdtfFieldOwnership::MvrProjectInstanceLevel,
     GdtfFieldValueKind::HostProjectValue, "fixture_instance_name", "Name",
     true, false, false, false},
    {GdtfFieldId::FixtureTypeName, GdtfFieldOwnership::GdtfTypeLevel,
     GdtfFieldValueKind::DocumentValue, "fixture_type_name", "Type", true,
     true, false, false},
    {GdtfFieldId::Layer, GdtfFieldOwnership::MvrProjectInstanceLevel,
     GdtfFieldValueKind::HostProjectValue, "layer", "Layer", true, false,
     false, false},
    {GdtfFieldId::HangPosition, GdtfFieldOwnership::MvrProjectInstanceLevel,
     GdtfFieldValueKind::HostProjectValue, "hang_position", "Hang position",
     true, false, false, false},
    {GdtfFieldId::Universe, GdtfFieldOwnership::MvrProjectInstanceLevel,
     GdtfFieldValueKind::HostProjectValue, "universe", "Universe", true,
     false, false, false},
    {GdtfFieldId::DmxAddress, GdtfFieldOwnership::MvrProjectInstanceLevel,
     GdtfFieldValueKind::HostProjectValue, "dmx_address", "Channel", true,
     false, false, false},
    {GdtfFieldId::ModeName, GdtfFieldOwnership::ContextSpecific,
     GdtfFieldValueKind::ContextSelection, "mode_name", "Mode", true, true,
     false, false},
    {GdtfFieldId::ChannelCount, GdtfFieldOwnership::DerivedReadOnly,
     GdtfFieldValueKind::DerivedReadOnly, "channel_count", "Ch Count", false,
     false, true, false},
    {GdtfFieldId::SourceFileReference, GdtfFieldOwnership::ContextSpecific,
     GdtfFieldValueKind::ContextSelection, "source_file_reference", "Model File",
     true, true, false, false},
    {GdtfFieldId::PositionX, GdtfFieldOwnership::MvrProjectInstanceLevel,
     GdtfFieldValueKind::HostProjectValue, "position_x", "Position X", true,
     false, false, false},
    {GdtfFieldId::PositionY, GdtfFieldOwnership::MvrProjectInstanceLevel,
     GdtfFieldValueKind::HostProjectValue, "position_y", "Position Y", true,
     false, false, false},
    {GdtfFieldId::PositionZ, GdtfFieldOwnership::MvrProjectInstanceLevel,
     GdtfFieldValueKind::HostProjectValue, "position_z", "Position Z", true,
     false, false, false},
    {GdtfFieldId::Roll, GdtfFieldOwnership::MvrProjectInstanceLevel,
     GdtfFieldValueKind::HostProjectValue, "roll", "Roll", true, false,
     false, false},
    {GdtfFieldId::Pitch, GdtfFieldOwnership::MvrProjectInstanceLevel,
     GdtfFieldValueKind::HostProjectValue, "pitch", "Pitch", true, false,
     false, false},
    {GdtfFieldId::Yaw, GdtfFieldOwnership::MvrProjectInstanceLevel,
     GdtfFieldValueKind::HostProjectValue, "yaw", "Yaw", true, false,
     false, false},
    {GdtfFieldId::PowerConsumption, GdtfFieldOwnership::GdtfTypeLevel,
     GdtfFieldValueKind::DocumentValue, "power_consumption", "Power", true,
     true, false, false},
    {GdtfFieldId::Weight, GdtfFieldOwnership::GdtfTypeLevel,
     GdtfFieldValueKind::DocumentValue, "weight", "Weight", true, true, false,
     false},
    {GdtfFieldId::FixtureCategory,
     GdtfFieldOwnership::ProjectClassificationOverride,
     GdtfFieldValueKind::HostProjectValue, "fixture_category", "Category", true,
     false, false, false},
    {GdtfFieldId::VisualColor,
     GdtfFieldOwnership::ProjectClassificationOverride,
     GdtfFieldValueKind::HostProjectValue, "visual_color", "Visual color", true,
     false, false, false},
    {GdtfFieldId::MvrFixtureColor, GdtfFieldOwnership::MvrProjectInstanceLevel,
     GdtfFieldValueKind::HostProjectValue, "mvr_fixture_color", "MVR color",
     true, false, false, false},
    {GdtfFieldId::Manufacturer, GdtfFieldOwnership::GdtfTypeLevel,
     GdtfFieldValueKind::DocumentValue, "manufacturer", "Manufacturer", true,
     true, false, false},
    {GdtfFieldId::ModelName, GdtfFieldOwnership::GdtfTypeLevel,
     GdtfFieldValueKind::DocumentValue, "model_name", "Model", true, true,
     false, false},
    {GdtfFieldId::TrussName, GdtfFieldOwnership::MvrProjectInstanceLevel,
     GdtfFieldValueKind::HostProjectValue, "truss_name", "Name", true, false,
     false, false},
    {GdtfFieldId::TrussLength, GdtfFieldOwnership::GdtfTypeLevel,
     GdtfFieldValueKind::DocumentValue, "truss_length", "Length", true, true,
     false, false},
    {GdtfFieldId::TrussWidth, GdtfFieldOwnership::GdtfTypeLevel,
     GdtfFieldValueKind::DocumentValue, "truss_width", "Width", true, true,
     false, false},
    {GdtfFieldId::TrussHeight, GdtfFieldOwnership::GdtfTypeLevel,
     GdtfFieldValueKind::DocumentValue, "truss_height", "Height", true, true,
     false, false},
    {GdtfFieldId::FixtureTypeDescription, GdtfFieldOwnership::GdtfTypeLevel,
     GdtfFieldValueKind::DocumentValue, "fixture_type_description", "Description",
     true, true, false, false},
    {GdtfFieldId::TrussCrossSectionType, GdtfFieldOwnership::GdtfTypeLevel,
     GdtfFieldValueKind::DocumentValue, "truss_cross_section_type", "Cross-section type",
     true, true, false, false},
    {GdtfFieldId::TrussCrossSection, GdtfFieldOwnership::GdtfTypeLevel,
     GdtfFieldValueKind::DocumentValue, "truss_cross_section", "Truss cross-section name",
     true, true, false, false},
    {GdtfFieldId::TrussLoad, GdtfFieldOwnership::DerivedReadOnly,
     GdtfFieldValueKind::DerivedReadOnly, "truss_load", "Load", false, false,
     true, false},
}};

} // namespace

// Finds the descriptor for a stable GDTF editor field identifier.
const GdtfFieldDescriptor *FindGdtfFieldDescriptor(GdtfFieldId id) {
  for (const auto &descriptor : kDescriptors) {
    if (descriptor.id == id)
      return &descriptor;
  }
  return nullptr;
}

// Returns the fields currently shown by Edit Fixture in table-column order.
std::vector<GdtfFieldDescriptor> CurrentFixtureEditFieldDescriptors() {
  return {{*FindGdtfFieldDescriptor(GdtfFieldId::FixtureId),
           *FindGdtfFieldDescriptor(GdtfFieldId::FixtureInstanceName),
           *FindGdtfFieldDescriptor(GdtfFieldId::FixtureTypeName),
           *FindGdtfFieldDescriptor(GdtfFieldId::Layer),
           *FindGdtfFieldDescriptor(GdtfFieldId::HangPosition),
           *FindGdtfFieldDescriptor(GdtfFieldId::Universe),
           *FindGdtfFieldDescriptor(GdtfFieldId::DmxAddress),
           *FindGdtfFieldDescriptor(GdtfFieldId::ModeName),
           *FindGdtfFieldDescriptor(GdtfFieldId::ChannelCount),
           *FindGdtfFieldDescriptor(GdtfFieldId::SourceFileReference),
           *FindGdtfFieldDescriptor(GdtfFieldId::PositionX),
           *FindGdtfFieldDescriptor(GdtfFieldId::PositionY),
           *FindGdtfFieldDescriptor(GdtfFieldId::PositionZ),
           *FindGdtfFieldDescriptor(GdtfFieldId::Roll),
           *FindGdtfFieldDescriptor(GdtfFieldId::Pitch),
           *FindGdtfFieldDescriptor(GdtfFieldId::Yaw),
           *FindGdtfFieldDescriptor(GdtfFieldId::FixtureTypeDescription),
           *FindGdtfFieldDescriptor(GdtfFieldId::PowerConsumption),
           *FindGdtfFieldDescriptor(GdtfFieldId::Weight),
           *FindGdtfFieldDescriptor(GdtfFieldId::FixtureCategory),
           *FindGdtfFieldDescriptor(GdtfFieldId::VisualColor),
           *FindGdtfFieldDescriptor(GdtfFieldId::MvrFixtureColor)}};
}

// Returns the fields currently shown by Edit Truss in table-column order plus cross section.
std::vector<GdtfFieldDescriptor> CurrentTrussEditFieldDescriptors() {
  return {{*FindGdtfFieldDescriptor(GdtfFieldId::TrussName),
           *FindGdtfFieldDescriptor(GdtfFieldId::Layer),
           *FindGdtfFieldDescriptor(GdtfFieldId::SourceFileReference),
           *FindGdtfFieldDescriptor(GdtfFieldId::HangPosition),
           *FindGdtfFieldDescriptor(GdtfFieldId::PositionX),
           *FindGdtfFieldDescriptor(GdtfFieldId::PositionY),
           *FindGdtfFieldDescriptor(GdtfFieldId::PositionZ),
           *FindGdtfFieldDescriptor(GdtfFieldId::Roll),
           *FindGdtfFieldDescriptor(GdtfFieldId::Pitch),
           *FindGdtfFieldDescriptor(GdtfFieldId::Yaw),
           *FindGdtfFieldDescriptor(GdtfFieldId::Manufacturer),
           *FindGdtfFieldDescriptor(GdtfFieldId::ModelName),
           *FindGdtfFieldDescriptor(GdtfFieldId::TrussLength),
           *FindGdtfFieldDescriptor(GdtfFieldId::TrussWidth),
           *FindGdtfFieldDescriptor(GdtfFieldId::TrussHeight),
           *FindGdtfFieldDescriptor(GdtfFieldId::Weight),
           *FindGdtfFieldDescriptor(GdtfFieldId::FixtureTypeDescription),
           *FindGdtfFieldDescriptor(GdtfFieldId::TrussLoad),
           *FindGdtfFieldDescriptor(GdtfFieldId::TrussCrossSectionType),
           *FindGdtfFieldDescriptor(GdtfFieldId::TrussCrossSection)}};
}

// Reports whether a field has explicit storage in the GDTF edit session model.
bool IsGdtfSessionValueSupported(GdtfFieldId id) {
  const auto *descriptor = FindGdtfFieldDescriptor(id);
  return descriptor && descriptor->sessionValueSupported;
}

// Converts a field ownership enum to a stable diagnostic string.
const char *ToString(GdtfFieldOwnership ownership) {
  switch (ownership) {
  case GdtfFieldOwnership::GdtfTypeLevel:
    return "GDTF type-level";
  case GdtfFieldOwnership::MvrProjectInstanceLevel:
    return "MVR/project instance-level";
  case GdtfFieldOwnership::DerivedReadOnly:
    return "derived/read-only";
  case GdtfFieldOwnership::ProjectClassificationOverride:
    return "project classification/override";
  case GdtfFieldOwnership::ContextSpecific:
    return "context-specific";
  case GdtfFieldOwnership::UnsupportedFuture:
    return "unsupported/future";
  }
  return "unknown";
}

// Converts a field value-kind enum to a stable diagnostic string.
const char *ToString(GdtfFieldValueKind defaultValueKind) {
  switch (defaultValueKind) {
  case GdtfFieldValueKind::DocumentValue:
    return "document value";
  case GdtfFieldValueKind::ContextSelection:
    return "context selection";
  case GdtfFieldValueKind::DerivedReadOnly:
    return "derived/read-only";
  case GdtfFieldValueKind::HostProjectValue:
    return "host/project value";
  case GdtfFieldValueKind::UnsupportedFuture:
    return "unsupported/future";
  }
  return "unknown";
}

} // namespace gdtf
