/* This file is part of Perastage. */
#pragma once
#include "fixture_network_service.h"
#include <filesystem>
namespace fixture_network {
std::optional<WireNames> ReadWireNames(const std::filesystem::path &gdtfPath);
// Read-only evidence; never changes source GDTF archives or authored assignments.
Result InspectReferences(const Fixture &fixture, const std::filesystem::path &gdtfPath);
// Publishes a resource rebind only after all Network names can be verified.
Result Rebind(Fixture &fixture, const std::string &reference,
              const std::filesystem::path &resolvedPath);
}
