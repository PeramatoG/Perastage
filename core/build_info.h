#pragma once

#include <string_view>

namespace perastage::build_info {

std::string_view appVersion() noexcept;
std::string_view appVersionDisplay() noexcept;
std::string_view gitCommit() noexcept;
std::string_view buildTimestampUtc() noexcept;
std::string_view buildConfiguration() noexcept;
std::string_view targetPlatform() noexcept;
std::string_view compilerDescription() noexcept;

} // namespace perastage::build_info
