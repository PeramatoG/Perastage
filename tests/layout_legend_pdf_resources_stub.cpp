#include "symbols/fixture_symbol_availability.h"
#include "symbols/project_fixture_symbol_runtime.h"
#include "projectutils.h"
#include "logger.h"

// Only resource IO is faked: the test runs the production PDF exporter.
namespace symbol_cache {
std::shared_ptr<const PerastageSvgSymbolData> LoadUsableFixtureSymbol(
    const std::string &source, SymbolViewKind view, std::string *,
    FixtureSymbolResolutionPurpose) {
  if (source != "legend-test.gdtf" && source != "pstg-symbol:test:content") return nullptr;
  auto svg = std::make_shared<PerastageSvgSymbolData>();
  svg->viewKind = view;
  svg->viewBoxWidth = 10;
  svg->viewBoxHeight = 10;
  svg->offsetXmm = 3;
  svg->offsetYmm = -2;
  // A distinct vector color-independent shape for each view.
  const double width = view == SymbolViewKind::Right ? 7 : 10;
  svg->fills.push_back({{{0, 0}, {width, 0}, {width, 10}, {0, 10}}, {}});
  return svg;
}
}
namespace symbols {
bool IsProjectFixtureSymbolSource(std::string_view source) {
  return source.starts_with("pstg-symbol:");
}
}
namespace ProjectUtils {
std::filesystem::path GetResourceRoot() { return PERASTAGE_TEST_RESOURCE_DIR; }
}
Logger::Logger() = default;
Logger::~Logger() = default;
Logger &Logger::Instance() { static Logger logger; return logger; }
void Logger::Log(std::string) {}
