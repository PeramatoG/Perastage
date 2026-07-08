#include "gdtf_edit_session.h"

#include <optional>
#include <utility>

namespace gdtf {
namespace {

// Reports whether a numeric optional value is negative.
bool IsNegative(const std::optional<float> &value) {
  return value.has_value() && *value < 0.0f;
}

// Reports whether a capability supports editable session storage.
bool SupportsEditableSessionValue(const GdtfFieldCapability &capability) {
  return capability.editable &&
         (capability.operation == GdtfFieldEditOperation::DocumentMutation ||
          capability.operation == GdtfFieldEditOperation::ContextSelection);
}

// Reports whether a field capability should participate in validation.
bool SupportsValidation(const GdtfEditorContext &context, GdtfFieldId fieldId) {
  const auto *capability = FindFieldCapability(context, fieldId);
  return capability && capability->visible && capability->editable &&
         IsGdtfSessionValueSupported(fieldId);
}

} // namespace

// Creates a non-GUI edit session from explicit host context data.
GdtfEditSession::GdtfEditSession(GdtfEditorContext context)
    : context_(std::move(context)), initialValues_(context_.initialValues),
      currentValues_(context_.initialValues) {}

// Returns the immutable GDTF document associated with the session.
const GdtfDocument &GdtfEditSession::Document() const {
  return context_.document;
}

// Returns the initial editable value snapshot.
const GdtfEditableValues &GdtfEditSession::InitialValues() const {
  return initialValues_;
}

// Returns the current editable value snapshot.
const GdtfEditableValues &GdtfEditSession::CurrentValues() const {
  return currentValues_;
}

// Returns host context metadata without exposing GUI objects.
const GdtfEditorContext &GdtfEditSession::Context() const { return context_; }

// Applies a user edit and updates field-level dirty tracking.
bool GdtfEditSession::SetValue(GdtfFieldId fieldId, const std::string &value) {
  if (!context_.editingAllowed)
    return false;
  const auto *capability = FindFieldCapability(context_, fieldId);
  if (!capability || !SupportsEditableSessionValue(*capability) ||
      !IsGdtfSessionValueSupported(fieldId))
    return false;
  auto nextValues = currentValues_;
  if (!SetEditableValue(nextValues, fieldId, value))
    return false;
  currentValues_ = std::move(nextValues);
  RecomputeDirtyFields();
  return true;
}

// Reports whether any editable field differs from its initial value.
bool GdtfEditSession::IsDirty() const { return !dirtyFields_.empty(); }

// Reports whether a specific field differs from its initial value.
bool GdtfEditSession::IsFieldDirty(GdtfFieldId fieldId) const {
  return dirtyFields_.count(fieldId) > 0;
}

// Returns the current field-level dirty set.
const std::set<GdtfFieldId> &GdtfEditSession::DirtyFields() const {
  return dirtyFields_;
}

// Reports whether an explicit host apply/save operation is needed.
bool GdtfEditSession::RequiresApply() const { return IsDirty(); }

// Validates current values without using GUI or project services.
std::vector<GdtfValidationDiagnostic> GdtfEditSession::Validate() const {
  std::vector<GdtfValidationDiagnostic> diagnostics;
  if (SupportsValidation(context_, GdtfFieldId::Weight) &&
      IsNegative(currentValues_.weightKg))
    diagnostics.push_back({GdtfFieldId::Weight, "Weight cannot be negative."});
  if (SupportsValidation(context_, GdtfFieldId::PowerConsumption) &&
      IsNegative(currentValues_.powerConsumptionW))
    diagnostics.push_back({GdtfFieldId::PowerConsumption,
                           "Power consumption cannot be negative."});
  if (SupportsValidation(context_, GdtfFieldId::TrussLength) &&
      IsNegative(currentValues_.trussLengthMm))
    diagnostics.push_back(
        {GdtfFieldId::TrussLength, "Truss length cannot be negative."});
  if (SupportsValidation(context_, GdtfFieldId::TrussWidth) &&
      IsNegative(currentValues_.trussWidthMm))
    diagnostics.push_back(
        {GdtfFieldId::TrussWidth, "Truss width cannot be negative."});
  if (SupportsValidation(context_, GdtfFieldId::TrussHeight) &&
      IsNegative(currentValues_.trussHeightMm))
    diagnostics.push_back(
        {GdtfFieldId::TrussHeight, "Truss height cannot be negative."});
  if (SupportsValidation(context_, GdtfFieldId::FixtureTypeName) &&
      currentValues_.fixtureTypeName && currentValues_.fixtureTypeName->empty())
    diagnostics.push_back(
        {GdtfFieldId::FixtureTypeName, "Fixture type name cannot be empty."});
  return diagnostics;
}

// Restores current values to the initial snapshot and clears dirty tracking.
void GdtfEditSession::Reset() {
  currentValues_ = initialValues_;
  dirtyFields_.clear();
}

// Builds a GUI-independent apply request for the host adapter.
GdtfApplyRequest GdtfEditSession::BuildApplyRequest() const {
  GdtfApplyRequest request;
  request.contextKind = context_.kind;
  request.sourcePath = context_.sourcePath;
  request.writePolicy = context_.writePolicy;
  request.values = currentValues_;
  for (const auto fieldId : dirtyFields_) {
    const auto *capability = FindFieldCapability(context_, fieldId);
    if (!capability)
      continue;
    if (capability->operation == GdtfFieldEditOperation::DocumentMutation)
      request.changedDocumentFields.insert(fieldId);
    else if (capability->operation == GdtfFieldEditOperation::ContextSelection)
      request.changedContextFields.insert(fieldId);
  }
  return request;
}

// Recomputes dirty fields from capabilities and editable value snapshots.
void GdtfEditSession::RecomputeDirtyFields() {
  dirtyFields_.clear();
  for (const auto &[fieldId, capability] : context_.fieldCapabilities) {
    if (!SupportsEditableSessionValue(capability) ||
        !IsGdtfSessionValueSupported(fieldId))
      continue;
    if (GetEditableValue(initialValues_, fieldId) !=
        GetEditableValue(currentValues_, fieldId))
      dirtyFields_.insert(fieldId);
  }
}

} // namespace gdtf
