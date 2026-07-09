#pragma once

#include "gdtf_editor_context.h"

#include <set>
#include <string>
#include <vector>

namespace gdtf {

struct GdtfValidationDiagnostic {
  GdtfFieldId fieldId = GdtfFieldId::FixtureTypeName;
  std::string message;
};

class GdtfEditSession {
public:
  explicit GdtfEditSession(GdtfEditorContext context);

  const GdtfDocument &Document() const;
  const GdtfEditableValues &InitialValues() const;
  const GdtfEditableValues &CurrentValues() const;
  const GdtfEditorContext &Context() const;
  bool SetValue(GdtfFieldId fieldId, const std::string &value);
  bool IsDirty() const;
  bool IsFieldDirty(GdtfFieldId fieldId) const;
  const std::set<GdtfFieldId> &DirtyFields() const;
  bool RequiresApply() const;
  std::vector<GdtfValidationDiagnostic> Validate() const;
  void Reset();
  void AcceptCurrentValues();
  void RebindContextPreservingValues(GdtfEditorContext context);
  GdtfApplyRequest BuildApplyRequest() const;

private:
  void RecomputeDirtyFields();

  GdtfEditorContext context_;
  GdtfEditableValues initialValues_;
  GdtfEditableValues currentValues_;
  std::set<GdtfFieldId> dirtyFields_;
};

} // namespace gdtf
