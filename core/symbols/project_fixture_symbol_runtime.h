#pragma once

#include "PerastageSvgSymbol.h"
#include "project_fixture_symbols.h"

#include <functional>
#include <memory>
#include <string_view>

namespace symbols {

// Composition installs the current project owner; the runtime cache owns no
// authoritative symbol data and is safe to discard at any time.
using ProjectFixtureSymbolProvider =
    std::function<const ProjectFixtureSymbolStore *()>;
void InstallProjectFixtureSymbolProvider(ProjectFixtureSymbolProvider provider);

// Pins immutable project data for reads on the current thread. Capture the
// store copy on its owning thread before starting background work. A null
// snapshot deliberately exposes no project symbols; it never uses the active
// project provider. Nested scopes restore the previous read context.
class ScopedProjectFixtureSymbolReadContext {
public:
  explicit ScopedProjectFixtureSymbolReadContext(
      std::shared_ptr<const ProjectFixtureSymbolStore> snapshot);
  ~ScopedProjectFixtureSymbolReadContext();
  ScopedProjectFixtureSymbolReadContext(
      const ScopedProjectFixtureSymbolReadContext &) = delete;
  ScopedProjectFixtureSymbolReadContext &operator=(
      const ScopedProjectFixtureSymbolReadContext &) = delete;

private:
  std::shared_ptr<const ProjectFixtureSymbolStore> previousSnapshot_;
  bool previousContextActive_ = false;
};

bool IsProjectFixtureSymbolSource(std::string_view source);
std::string BuildProjectFixtureSymbolSource(const Fixture &fixture);
bool InspectProjectFixtureSymbolSource(
    const std::string &source, FixtureSymbolResourceInspection &inspection);
std::shared_ptr<const PerastageSvgSymbolData> LoadProjectFixtureSymbolSource(
    const std::string &source, SymbolViewKind view,
    std::string *errorDetails = nullptr);
void ClearProjectFixtureSymbolRuntimeCache();

} // namespace symbols
