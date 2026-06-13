#pragma once

#include "types.h"

#include <functional>
#include <string>

class wxWindow;

namespace scene_object_primitives {

struct SphereRequest {
  std::string name = "Sphere";
  double radiusMeters = 1.0;
  int quantity = 1;
};

struct CubeRequest {
  std::string name = "Cube";
  double lengthMeters = 1.0;
  double heightMeters = 1.0;
  double widthMeters = 1.0;
  int quantity = 1;
};

struct CylinderRequest {
  std::string name = "Cylinder";
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

struct PrimitivePlacementRequest {
  double positionXMeters = 0.0;
  double positionYMeters = 0.0;
  double positionZMeters = 0.0;
  double rotationXDegrees = 0.0;
  double rotationYDegrees = 0.0;
  double rotationZDegrees = 0.0;
};

using PrimitiveApplyCallback = std::function<void()>;

bool ShowSphereDialog(wxWindow *parent, SphereRequest &outRequest);
bool ShowCubeDialog(wxWindow *parent, CubeRequest &outRequest);
bool ShowCylinderDialog(wxWindow *parent, CylinderRequest &outRequest);
bool ShowSphereEditDialog(wxWindow *parent, SphereRequest &inOutRequest,
                          PrimitivePlacementRequest &inOutPlacement,
                          const PrimitiveApplyCallback &applyCallback = {});
bool ShowCubeEditDialog(wxWindow *parent, CubeRequest &inOutRequest,
                        PrimitivePlacementRequest &inOutPlacement,
                        const PrimitiveApplyCallback &applyCallback = {});
bool ShowCylinderEditDialog(wxWindow *parent, CylinderRequest &inOutRequest,
                            PrimitivePlacementRequest &inOutPlacement,
                            const PrimitiveApplyCallback &applyCallback = {});
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
