#pragma once

#include "truss_attachment_paths.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace fixture_line_distribution {

struct Line {
  std::array<float, 3> start{};
  std::array<float, 3> end{};
  std::string trussUuid;
};

enum class ResolveError {
  None,
  TooFewFixtures,
  MissingFixture,
  NotOnSameTruss
};

struct ResolveResult {
  std::optional<Line> line;
  ResolveError error = ResolveError::None;
};

// Resolves the shared connected attachment path containing every fixture.
ResolveResult ResolveSelectedLine(
    const MvrScene &scene, const std::vector<std::string> &fixtureUuids,
    const std::vector<truss_attachment_paths::Path> &attachmentPaths,
    float toleranceMm = 75.0f);

// Projects a world point onto a finite truss line.
std::array<float, 3> ProjectOntoLine(const Line &line,
                                     const std::array<float, 3> &point);

// Distributes fixtures in selection order between two positions on a line.
bool Apply(MvrScene &scene, const std::vector<std::string> &fixtureUuids,
           const std::array<float, 3> &start, const std::array<float, 3> &end,
           bool includeEndpointMargins);

} // namespace fixture_line_distribution
