#pragma once

#include "fixture.h"
#include "gdtf_edit_session.h"

namespace gdtf {

struct ProjectFixtureGdtfContextInput {
  Fixture fixture;
  std::filesystem::path resolvedGdtfPath;
  // Resolved editor-facing source selection; fixture.gdtfSpec remains portable.
  std::string editorSourceFileReference;
  GdtfSourceKind sourceKind = GdtfSourceKind::Unknown;
  GdtfWritePolicy writePolicy = GdtfWritePolicy::CreateDerivativeBeforeMutation;
  GdtfDocument document;
};

GdtfEditorContext BuildProjectFixtureGdtfEditorContext(
    const ProjectFixtureGdtfContextInput &input);
GdtfEditSession
BuildProjectFixtureGdtfEditSession(const ProjectFixtureGdtfContextInput &input);

} // namespace gdtf
