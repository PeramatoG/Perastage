/* This file is part of Perastage. */
#pragma once
#include <filesystem>
#include <string>
namespace mvr {
// Filename aliases are an explicit legacy-native policy, never proof that a
// failed acquired/imported source resource equals an unrelated namesake.
std::filesystem::path
ResolveExistingSceneResource(const std::string &reference,
                             const std::filesystem::path &base,
                             bool allowFilenameAlias);
} // namespace mvr
