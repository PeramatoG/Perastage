#include "configservices.h"

#include <cassert>

int main() {
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
