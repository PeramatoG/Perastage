#pragma once

#include "../models/mvrscene.h"

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

// Resolves the single truss line containing every selected fixture.
ResolveResult ResolveSelectedLine(const MvrScene &scene,
                                  const std::vector<std::string> &fixtureUuids,
                                  float toleranceMm = 75.0f);

// Projects a world point onto a finite truss line.
std::array<float, 3> ProjectOntoLine(const Line &line,
                                     const std::array<float, 3> &point);

// Distributes fixtures in selection order between two positions on a line.
bool Apply(MvrScene &scene, const std::vector<std::string> &fixtureUuids,
           const std::array<float, 3> &start, const std::array<float, 3> &end,
           bool includeEndpointMargins);

} // namespace fixture_line_distribution
