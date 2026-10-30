/* This file is part of Perastage. */
#pragma once
#include "fixture_network_service.h"
#include <filesystem>
struct Truss;
namespace fixture_network {
std::optional<WireNames> ReadWireNames(const std::filesystem::path &gdtfPath);
Result InspectTrussReferences(const Truss &truss, const std::filesystem::path &gdtfPath);
Result RebindNode(Truss &truss, const std::string &reference, const std::filesystem::path &path);
// Read-only evidence; never changes source GDTF archives or authored assignments.
Result InspectReferences(const Fixture &fixture, const std::filesystem::path &gdtfPath);
template <class Node>
Result InspectNodeReferences(const Node &node, const std::filesystem::path &path) {
  if (auto result = Validate(node); !result) return result;
  if (!node.dmxAddresses || node.dmxAddresses->networks.empty()) return {};
  return CheckReferences(node, ReadWireNames(path));
}
template <class Node>
Result RebindNode(Node &node, const std::string &reference, const std::filesystem::path &path) {
  if (auto binding = CheckBindingReference(node, reference); !binding) return binding;
  auto result = InspectNodeReferences(node, path);
  if (result) node.gdtfSpec = reference;
  return result;
}
// Publishes a resource rebind only after all Network names can be verified.
Result Rebind(Fixture &fixture, const std::string &reference,
              const std::filesystem::path &resolvedPath);
}
