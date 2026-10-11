#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace user_manual {
inline constexpr std::string_view kConsoleHelpPage = "shortcuts-and-command-bar.md";
inline constexpr std::string_view kConsoleHelpBegin = "<!-- PERASTAGE_CONSOLE_HELP_BEGIN -->";
inline constexpr std::string_view kConsoleHelpEnd = "<!-- PERASTAGE_CONSOLE_HELP_END -->";
inline constexpr const char *kConsoleHelpUnavailable =
    "Console help is unavailable. Open Help > Online Documentation.";

// Extract exactly one nonempty, ordered marked section, independent of headings.
std::optional<std::string> ExtractConsoleHelp(std::string_view markdown);
// manualRoot follows the existing packaged help/ resource contract. Always English.
std::string LoadConsoleHelp(const std::filesystem::path &manualRoot);
} // namespace user_manual
