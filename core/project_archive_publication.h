#pragma once
#include <filesystem>
#include <string>

namespace project_archive {
// Atomic replacement only: a failed publication never partially overwrites the
// previous target. Callers must close all archive handles before publication.
bool Publish(const std::filesystem::path &temporary,
             const std::filesystem::path &target, std::string &error);
}
