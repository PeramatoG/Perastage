#pragma once

#include "gdtf/gdtf_physical_properties_panel.h"
#include "gdtf/gdtf_type_identity_panel.h"
#include "gdtf/editor/gdtf_edit_session.h"

#include <optional>
#include <string>

namespace gui::gdtf_binding {

// Maps a reusable identity panel field to the core session field identifier.
inline std::optional<gdtf::GdtfFieldId>
ToFieldId(GdtfTypeIdentityField field) {
  switch (field) {
  case GdtfTypeIdentityField::FixtureTypeName:
    return gdtf::GdtfFieldId::FixtureTypeName;
  case GdtfTypeIdentityField::Manufacturer:
    return gdtf::GdtfFieldId::Manufacturer;
  case GdtfTypeIdentityField::ModelName:
    return gdtf::GdtfFieldId::ModelName;
  case GdtfTypeIdentityField::SourceFileReference:
    return gdtf::GdtfFieldId::SourceFileReference;
  }
  return std::nullopt;
}

// Maps a reusable physical panel field to the core session field identifier.
inline std::optional<gdtf::GdtfFieldId>
ToFieldId(GdtfPhysicalPropertyField field) {
  switch (field) {
  case GdtfPhysicalPropertyField::PowerConsumption:
    return gdtf::GdtfFieldId::PowerConsumption;
  case GdtfPhysicalPropertyField::Weight:
    return gdtf::GdtfFieldId::Weight;
  case GdtfPhysicalPropertyField::Length:
    return gdtf::GdtfFieldId::TrussLength;
  case GdtfPhysicalPropertyField::Width:
    return gdtf::GdtfFieldId::TrussWidth;
  case GdtfPhysicalPropertyField::Height:
    return gdtf::GdtfFieldId::TrussHeight;
  case GdtfPhysicalPropertyField::CrossSection:
    return gdtf::GdtfFieldId::TrussCrossSection;
  }
  return std::nullopt;
}

// Formats a session value for wx presentation while preserving empty fallbacks.
inline std::string ValueText(const gdtf::GdtfEditableValues &values,
                             gdtf::GdtfFieldId fieldId,
                             const std::string &fallback = {}) {
  return gdtf::GetEditableValue(values, fieldId).value_or(fallback);
}

// Reports whether a session field is editable in the active context.
inline bool IsEditable(const gdtf::GdtfEditSession &session,
                       gdtf::GdtfFieldId fieldId) {
  const auto *capability = gdtf::FindFieldCapability(session.Context(), fieldId);
  return capability && capability->visible && capability->editable;
}

} // namespace gui::gdtf_binding
