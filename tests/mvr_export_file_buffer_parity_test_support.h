#pragma once

#include <filesystem>

class ConfigManager;

// Verifies semantic package parity between public file and buffer exports.
void VerifyMvrExportFileBufferParity(ConfigManager &config,
                                     const std::filesystem::path &directory);
