#include "interaction/selection_work_plane.h"
#include "interaction/selection_drag_math.h"
#include <cassert>
#include <cmath>

int main() {
  using alignment::Plane;
  assert(viewer3d::WorkPlaneForView(Viewer2DView::Top) == Plane::XY);
  assert(viewer3d::WorkPlaneForView(Viewer2DView::Bottom) == Plane::XY);
  assert(viewer3d::WorkPlaneForView(Viewer2DView::Front) == Plane::XZ);
  assert(viewer3d::WorkPlaneForView(Viewer2DView::Side) == Plane::YZ);
  for (auto plane : {Plane::XY,Plane::XZ,Plane::YZ}) {
    const auto normal = viewer3d::WorkPlaneNormal(plane);
    const int excluded = viewer3d::WorkPlaneNormalAxis(plane);
    assert(normal[excluded] == 1);
    const auto local = viewer3d::ProjectDragAxisToPlane({0.3f,0.4f,0.5f},plane);
    assert(local[excluded] == 0);
    assert(std::abs(std::hypot(local[0],local[1],local[2])-1) < 1e-6);
    std::array<float,3> excludedAxis{};
    excludedAxis[excluded] = 1;
    assert((viewer3d::ProjectDragAxisToPlane(excludedAxis,plane) == std::array<float,3>{}));
    const std::array<double,3> anchor{3,6,9};
    const auto intersection = viewer3d::IntersectRayWithPlane({0,0,0},{1,2,3},anchor,normal);
    assert(intersection && (*intersection)[excluded] == anchor[excluded]);
    std::array<double,3> parallel{1,1,1};
    parallel[excluded] = 0;
    assert(!viewer3d::IntersectRayWithPlane({0,0,0},parallel,anchor,normal));
  }
}
