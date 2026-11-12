/* This file is part of Perastage. License: GPL-3.0-or-later. */
#pragma once
#include "mvrscene.h"
#include "gdtf/gdtf_mode_channel_browser.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <set>

namespace dmx_patch {
using Universe = std::uint64_t;
struct Address { Universe universe; int channel; };
// Strict MVR syntax, using the shared address validator; no span normalization.
std::optional<Address> ReadAddress(const std::string &text);
struct Occupant {
  std::string uuid;
  int gdtfBreak = 1; // GDTF break 1 corresponds to MVR Address break 0.
  int offset = 0;
  int byteIndex = 0;
  std::string attribute;
  std::string functions;
  bool operator==(const Occupant &) const = default;
};
struct Cell {
  std::vector<Occupant> occupants;
  // Declared-offset preview only: excluded from physical occupancy/conflicts.
  std::vector<Occupant> provisional;
  bool Conflict() const;
};
enum class Issue { Unpatched, InvalidAddress, InvalidBreak, UnresolvedDefinition,
                   UnresolvedChannel, Overflow };
struct SourceChannel { std::string offsets, breakId, geometry; };
struct Diagnostic {
  std::string uuid;
  Issue issue;
  std::string detail;
  std::optional<SourceChannel> source;
};
struct FixtureInfo {
  std::string name, identifier, purpose;
  std::optional<std::string> colorHex;
};
struct Snapshot {
  Universe universe = 1;
  std::array<Cell, 512> cells;
  std::map<std::string, FixtureInfo> fixtures;
  std::set<Universe> universes;
  std::vector<Diagnostic> diagnostics;
  int occupied = 0;
  int conflicts = 0;
  int provisional = 0;
};
using DocumentResolver = std::function<std::shared_ptr<const gdtf::GdtfModeChannelDocument>(const Fixture &)>;
// Read-only projection. The resolver owns resource lookup/cache policy, never painting.
enum class Preview { ResolvedOnly, DeclaredOffsets };
Snapshot BuildSnapshot(const MvrScene &scene, Universe universe, const DocumentResolver &resolve,
                       Preview preview = Preview::ResolvedOnly);
} // namespace dmx_patch
