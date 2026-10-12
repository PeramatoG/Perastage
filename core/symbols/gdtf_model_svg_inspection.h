#pragma once

#include "symbol_view_kind.h"

#include <filesystem>
#include <string>
#include <vector>

namespace symbols {

enum class ModelSvgResourceState {
  Missing, Present, Malformed, Unreadable, Ambiguous
};

struct ModelSvgViewDefinition {
  SymbolViewKind view;
  std::string directory;
  std::string offsetXAttribute;
  std::string offsetYAttribute;
};

struct ModelSvgResourceInspection {
  SymbolViewKind view = SymbolViewKind::Top;
  std::string archivePath;
  bool present = false;
  bool legacyPerastage = false;
  ModelSvgResourceState state = ModelSvgResourceState::Missing;
  double offsetX = 0.0;
  double offsetY = 0.0;
  std::string diagnostic;
};

struct GdtfModelSvgInspection {
  std::string name;
  std::string file;
  double lengthMeters = 0.0;
  double widthMeters = 0.0;
  double heightMeters = 0.0;
  std::string primitiveType;
  bool ambiguousFile = false;
  std::vector<std::string> geometryReferences;
  std::vector<ModelSvgResourceInspection> views;
  std::vector<std::string> diagnostics;
};

struct GdtfModelSvgCatalog {
  bool success = false;
  std::string dataVersion;
  std::vector<GdtfModelSvgInspection> models;
  std::vector<std::string> diagnostics;
};

// Only published version semantics are enabled; unknown versions are diagnostic.
std::vector<ModelSvgViewDefinition> GdtfModelSvgViews(const std::string &dataVersion);
bool IsSafeGdtfModelResourceBasename(const std::string &file);

// Enumerates Models in document order without guessing ownership or converting SVGs.
GdtfModelSvgCatalog InspectGdtfModelSvgs(const std::filesystem::path &source);

} // namespace symbols
