#pragma once

class Viewer3DCamera;
class Viewer3DController;

namespace viewer3d {

bool FrameSceneInCamera(const Viewer3DController &controller, int viewportWidth,
                        int viewportHeight, Viewer3DCamera &camera);

} // namespace viewer3d
