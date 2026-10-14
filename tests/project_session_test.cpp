#include "configservices.h"

#include <cassert>

int main() {
  {
    ProjectSession session;
    const auto initial = session.GetRenderRevision();
    session.Touch();
    assert(session.GetRenderRevision() > initial);
    const auto state = session.CaptureDirtyState();
    const auto touched = session.GetRenderRevision();
    session.ResetDirty();
    assert(session.GetRenderRevision() > touched);
    const auto reset = session.GetRenderRevision();
    session.RestoreDirtyState(state);
    assert(session.GetRenderRevision() > reset);
    const auto restored = session.GetRenderRevision();
    session.MarkSaved();
    assert(session.GetRenderRevision() == restored);
  }
  ProjectSession session;
  assert(!session.IsDirty());
  session.Touch();
  assert(session.IsDirty());
  session.MarkSaved();
  assert(!session.IsDirty());

  Fixture f;
  f.uuid = "fixture-1";
  session.GetScene().fixtures[f.uuid] = f;
  assert(session.GetScene().fixtures.size() == 1);

  session.ResetDirty();
  assert(!session.IsDirty());
  return 0;
}
