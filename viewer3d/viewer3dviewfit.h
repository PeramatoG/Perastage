#pragma once

class Viewer3DCamera;
class ISelectionContext;

namespace viewer3d {

bool FrameSceneInCamera(const ISelectionContext &selectionContext, int viewportWidth,
                        int viewportHeight, Viewer3DCamera &camera);

} // namespace viewer3d
