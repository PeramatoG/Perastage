#pragma once

#include "inspection/inspection_contract.h"
#include "inspection/mvr_inspection.h"
#include "inspection/resource_inspection.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace gui::inspection {

// Represents one deterministic, toolkit-independent package tree node.
struct PackageTreeNode {
  std::string name;
  std::string archivePath;
  perastage::inspection::PackageEntryType entryType =
      perastage::inspection::PackageEntryType::Directory;
  perastage::inspection::ResourceKind resourceKind =
      perastage::inspection::ResourceKind::Binary;
  std::uint64_t size = 0;
  bool sizeKnown = false;
  bool pathSafe = false;
  bool syntheticFolder = true;
  bool rawReadSupported = false;
  bool textPreviewSupported = false;
  std::vector<PackageTreeNode> children;

  // Reports whether the unchanged archive path can be copied.
  bool CanCopyArchivePath() const;
};

// Builds a safe hierarchy solely from Inspection Core resource descriptors.
std::vector<PackageTreeNode> BuildPackageTree(
    const std::vector<perastage::inspection::ResourceDescriptor> &resources);

// Represents one deterministic, toolkit-independent MVR scene tree node.
struct SceneTreeNode {
  std::string kind;
  std::string uuid;
  std::string name;
  bool unresolved = false;
  std::vector<SceneTreeNode> children;

  bool operator==(const SceneTreeNode &) const = default;
};

// Projects all neutral scene descriptors without consulting the live scene.
std::vector<SceneTreeNode>
BuildSceneTree(const perastage::inspection::MvrInspectionSnapshot &snapshot);

// Represents one structured issue group without parsing diagnostic messages.
struct IssueGroup {
  perastage::inspection::DiagnosticSeverity severity =
      perastage::inspection::DiagnosticSeverity::Information;
  perastage::inspection::DiagnosticClassification classification =
      perastage::inspection::DiagnosticClassification::General;
  std::string code;
  std::size_t count = 0;

  bool operator==(const IssueGroup &) const = default;
};

// Groups diagnostics by stable structured severity, classification, and code.
std::vector<IssueGroup> BuildIssueGroups(
    const std::vector<perastage::inspection::Diagnostic> &diagnostics);

// Finds a literal query from a character selection with optional wrap-around.
std::optional<std::size_t> FindText(const std::string &text,
                                    const std::string &query,
                                    std::size_t selectionStart,
                                    std::size_t selectionEnd, bool forward);

// Parses a normalized splitter ratio and rejects invalid legacy values.
double ParseSplitterRatio(const std::optional<std::string> &stored,
                          double fallback);

// Serializes a clamped splitter ratio for DPI-independent persistence.
std::string FormatSplitterRatio(double ratio);

} // namespace gui::inspection
