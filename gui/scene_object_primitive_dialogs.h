#pragma once

#include "types.h"

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

struct ScreenEditRequest {
  double widthMeters = 8.0;
  double heightMeters = 5.0;
};

struct PipeEditRequest {
  double lengthMeters = 14.0;
};

bool ShowSphereDialog(wxWindow *parent, SphereRequest &outRequest);
bool ShowCubeDialog(wxWindow *parent, CubeRequest &outRequest);
bool ShowSphereEditDialog(wxWindow *parent, SphereRequest &inOutRequest);
bool ShowCubeEditDialog(wxWindow *parent, CubeRequest &inOutRequest);
bool ShowScreenEditDialog(wxWindow *parent, ScreenEditRequest &inOutRequest);
bool ShowPipeEditDialog(wxWindow *parent, PipeEditRequest &inOutRequest);

Matrix BuildSphereScaleTransform(double radiusMeters);
Matrix BuildCubeScaleTransform(double lengthMeters, double heightMeters,
                               double widthMeters);

} // namespace scene_object_primitives
