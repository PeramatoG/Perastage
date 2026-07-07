#include "gdtf_edit_session.h"

#include <array>

namespace gdtf {
namespace {

constexpr std::array<GdtfFieldId, 11> kEditableFields = {
    GdtfFieldId::FixtureTypeName,
    GdtfFieldId::Manufacturer,
    GdtfFieldId::ModelName,
    GdtfFieldId::ModeName,
    GdtfFieldId::Weight,
    GdtfFieldId::PowerConsumption,
    GdtfFieldId::TrussLength,
    GdtfFieldId::TrussWidth,
    GdtfFieldId::TrussHeight,
    GdtfFieldId::TrussCrossSection,
    GdtfFieldId::SourceFileReference};

// Reports whether a numeric optional value is negative.
bool IsNegative(const std::optional<float> &value) {
  return value.has_value() && *value < 0.0f;
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
  if (!SetEditableValue(currentValues_, fieldId, value))
    return false;
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
  if (IsNegative(currentValues_.weightKg))
    diagnostics.push_back({GdtfFieldId::Weight, "Weight cannot be negative."});
  if (IsNegative(currentValues_.powerConsumptionW))
    diagnostics.push_back({GdtfFieldId::PowerConsumption,
                           "Power consumption cannot be negative."});
  if (IsNegative(currentValues_.trussLengthMm))
    diagnostics.push_back(
        {GdtfFieldId::TrussLength, "Truss length cannot be negative."});
  if (IsNegative(currentValues_.trussWidthMm))
    diagnostics.push_back(
        {GdtfFieldId::TrussWidth, "Truss width cannot be negative."});
  if (IsNegative(currentValues_.trussHeightMm))
    diagnostics.push_back(
        {GdtfFieldId::TrussHeight, "Truss height cannot be negative."});
  if (currentValues_.fixtureTypeName && currentValues_.fixtureTypeName->empty())
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
  request.changedFields = dirtyFields_;
  return request;
}

// Recomputes dirty fields from the initial and current editable value
// snapshots.
void GdtfEditSession::RecomputeDirtyFields() {
  dirtyFields_.clear();
  for (const auto fieldId : kEditableFields) {
    if (GetEditableValue(initialValues_, fieldId) !=
        GetEditableValue(currentValues_, fieldId))
      dirtyFields_.insert(fieldId);
  }
}

} // namespace gdtf
