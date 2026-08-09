#pragma once

#include <array>
#include <string>
#include <vector>

#include "symbolcache.h"

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

struct RequiredFixtureSvgViewInspection {
  SymbolViewKind viewKind = SymbolViewKind::Top;
  std::string archivePath;
  bool usable = false;
  std::string diagnostic;
};

struct RequiredFixtureSvgSetInspection {
  std::array<RequiredFixtureSvgViewInspection, 4> views;
  bool usable = false;
  std::string diagnostic;
};

bool LoadPerastageSvgSymbolFromGdtf(const std::string &gdtfPath,
                                    SymbolViewKind requestedView,
                                    PerastageSvgSymbolData &out,
                                    std::string *errorDetails = nullptr);

bool InspectRequiredFixtureSvgSet(const std::string &gdtfPath,
                                  RequiredFixtureSvgSetInspection &inspection);
