#pragma once

#include <string>
#include <vector>

#include "fixture_symbol_resolution.h"

struct PerastageSvgPoint {
  double x = 0.0;
  double y = 0.0;
};

struct PerastageSvgPolyline {
  std::vector<PerastageSvgPoint> points;
};

struct PerastageSvgPolygon {
  std::vector<PerastageSvgPoint> points;
  std::vector<std::vector<PerastageSvgPoint>> holes;
};

struct PerastageSvgSymbolData {
  std::string sourcePath;
  SymbolViewKind viewKind = SymbolViewKind::Top;
  FixtureSymbolProvenance provenance = FixtureSymbolProvenance::None;
  FixtureSymbolResourceSet resourceSet = FixtureSymbolResourceSet::StandardGdtf;
  bool usedViewFallback = false;
  double viewBoxWidth = 0.0;
  double viewBoxHeight = 0.0;
  double offsetXmm = 0.0;
  double offsetYmm = 0.0;
  std::vector<PerastageSvgPolygon> fills;
  std::vector<PerastageSvgPolyline> strokes;

  bool IsValid() const {
    return viewBoxWidth > 0.0 && viewBoxHeight > 0.0 &&
           (!fills.empty() || !strokes.empty());
  }
};

bool LoadPerastageSvgSymbolFromGdtf(const std::string &gdtfPath,
                                    SymbolViewKind requestedView,
                                    PerastageSvgSymbolData &out,
                                    std::string *errorDetails = nullptr,
                                    FixtureSymbolResolutionPurpose purpose =
                                        FixtureSymbolResolutionPurpose::InternalRendering);

// Parses authoritative PSTG bytes without a GDTF archive or generator access.
bool ParsePerastageProjectSvgSymbol(const std::string &svg,
                                    PerastageSvgSymbolData &out,
                                    std::string *errorDetails = nullptr);

bool ParseFixtureSymbolSvg(const std::string &svg, PerastageSvgSymbolData &out,
                           std::string *errorDetails = nullptr);
