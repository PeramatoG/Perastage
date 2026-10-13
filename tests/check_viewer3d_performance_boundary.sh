#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
run_test_python - <<'PY'
from pathlib import Path
controller = Path('viewer3d/viewer3dcontroller.cpp').read_text()
assert 'ComputeSceneLayerMembershipFingerprint' not in controller
assert 'GetRenderRevision()' in controller
cache = Path('viewer3d/resources/runtime_mesh_cache.cpp').read_text()
resolve = cache.split('const Mesh &RuntimeMeshCache::Resolve', 1)[1].split('void RuntimeMeshCache::Queue', 1)[0]
assert 'BuildRuntimeRepresentation' not in resolve
assert 'PreparePending' in cache
budget = Path('viewer3d/resources/resource_work_budget.h').read_text()
for forbidden in ('wx/', 'SceneObject', 'Fixture *', 'glGen', 'glDelete'):
    assert forbidden not in budget
identity = Path('core/packaged_resource_identity.h').read_text()
assert 'wx/' not in identity and 'gui/' not in identity
panel = Path('viewer3d/viewer3dpanel.cpp').read_text()
assert 'Bind(wxEVT_VIEWER_REFRESH, &Viewer3DPanel::OnThreadRefresh, this)' in panel
assert 'Unbind(wxEVT_VIEWER_REFRESH, &Viewer3DPanel::OnThreadRefresh, this)' in panel
assert panel.index('SwapBuffers();') < panel.index('FinishPresentedFrame(')
assert 'SynchronizeRenderResources(framePlan.captureActive)' in Path('viewer2d/viewer2dpanel.cpp').read_text()
print('OK: progressive work, revision invalidation and deferred representations')
PY
