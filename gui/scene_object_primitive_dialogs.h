#pragma once

#include "types.h"

#include <string>

class wxWindow;

namespace scene_object_primitives {

struct SphereRequest {
  double radiusMeters = 1.0;
  int quantity = 1;
};

struct CubeRequest {
  double lengthMeters = 1.0;
  double heightMeters = 1.0;
  double widthMeters = 1.0;
  int quantity = 1;
};

struct CylinderRequest {
  double topRadiusMeters = 0.5;
  double bottomRadiusMeters = 0.5;
  double heightMeters = 1.0;
  int quantity = 1;
};

struct ScreenEditRequest {
  double widthMeters = 8.0;
  double heightMeters = 5.0;
};

struct PipeEditRequest {
  double lengthMeters = 14.0;
};

bool ShowSphereDialog(wxWindow *parent, SphereRequest &outRequest);
bool ShowCubeDialog(wxWindow *parent, CubeRequest &outRequest);
bool ShowCylinderDialog(wxWindow *parent, CylinderRequest &outRequest);
bool ShowSphereEditDialog(wxWindow *parent, SphereRequest &inOutRequest);
bool ShowCubeEditDialog(wxWindow *parent, CubeRequest &inOutRequest);
bool ShowCylinderEditDialog(wxWindow *parent, CylinderRequest &inOutRequest);
bool ShowScreenEditDialog(wxWindow *parent, ScreenEditRequest &inOutRequest);
bool ShowPipeEditDialog(wxWindow *parent, PipeEditRequest &inOutRequest);

Matrix BuildSphereScaleTransform(double radiusMeters);
Matrix BuildCubeScaleTransform(double lengthMeters, double heightMeters,
                               double widthMeters);
Matrix BuildCylinderScaleTransform(double radiusMeters, double heightMeters);
std::string BuildCylinderPrimitiveToken(double topRadiusMeters,
                                        double bottomRadiusMeters,
                                        double heightMeters);
CylinderRequest ParseCylinderPrimitiveToken(const std::string &token,
                                            const Matrix &fallbackTransform);

} // namespace scene_object_primitives
