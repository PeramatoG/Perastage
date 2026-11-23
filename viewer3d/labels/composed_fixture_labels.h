#pragma once
#include "iselectioncontext.h"
#include "viewer3d_types.h"
#include "fixture_label_composition.h"
void DrawComposedFixtureLabels(ISelectionContext &controller, int width, int height,
    Viewer2DView view, float zoom, const viewer2d::FixtureLabelOverrideMap &styles,
    viewer2d::FixtureLabelCompositionCache &cache,
    const ISelectionContext::ViewFrustumSnapshot &projection,
    const std::unordered_set<std::string> &hiddenLayers,
    const std::unordered_set<std::string> &hiddenFixtureTypes,
    bool cullOffscreen);
