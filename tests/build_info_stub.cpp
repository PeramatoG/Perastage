#include "../core/build_info.h"

namespace perastage::build_info {

// Returns a stable app version for focused unit tests.
std::string_view appVersion() noexcept { return "test"; }

// Returns a stable display version for focused unit tests.
std::string_view appVersionDisplay() noexcept { return "test"; }

// Returns a stable git commit for focused unit tests.
std::string_view gitCommit() noexcept { return "test"; }

// Returns a stable build timestamp for focused unit tests.
std::string_view buildTimestampUtc() noexcept { return "test"; }

// Returns a stable build configuration for focused unit tests.
std::string_view buildConfiguration() noexcept { return "test"; }

// Returns a stable target platform for focused unit tests.
std::string_view targetPlatform() noexcept { return "test"; }

// Returns a stable compiler description for focused unit tests.
std::string_view compilerDescription() noexcept { return "test"; }

} // namespace perastage::build_info
