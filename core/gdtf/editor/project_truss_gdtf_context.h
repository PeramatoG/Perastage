#pragma once

#include "gdtf_edit_session.h"
#include "truss.h"

namespace gdtf {

struct ProjectTrussGdtfContextInput {
  Truss truss;
  std::filesystem::path resolvedGdtfPath;
  GdtfSourceKind sourceKind = GdtfSourceKind::PerastageGeneratedDerivative;
  GdtfWritePolicy writePolicy = GdtfWritePolicy::ProjectControlledGeneration;
  GdtfDocument document;
};

GdtfEditorContext
BuildProjectTrussGdtfEditorContext(const ProjectTrussGdtfContextInput &input);
GdtfEditSession
BuildProjectTrussGdtfEditSession(const ProjectTrussGdtfContextInput &input);

} // namespace gdtf
