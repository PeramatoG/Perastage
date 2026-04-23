#pragma once

#include <string>

#include "fixture.h"
#include "mvrscene.h"

namespace gui::fixtures {

struct FixtureGdtfResolution {
  std::string scenePath;
  std::string libraryPath;
  std::string selectedPath;
};

bool ResolveFixtureGdtfDeterministic(const Fixture &fixture,
                                     const MvrScene &scene,
                                     FixtureGdtfResolution &resolution,
                                     std::string &errorMessage,
                                     const std::string &traceContext = {});

} // namespace gui::fixtures
