#include "inspection/inspector_source_document.h"

#include <cassert>

using namespace gui::inspection;

// Verifies selected documents never replace retained primary source authority.
int main() {
  InspectorSourceDocumentState state;
  state.SetPrimary({"description.xml", "<root/>", InspectorSourceSyntax::Xml});
  assert(state.Primary() && state.Displayed()->primary);
  state.ShowSelected({"gobo.svg", "<svg/>", InspectorSourceSyntax::Xml});
  assert(state.Displayed()->entryPath == "gobo.svg");
  assert(state.Primary()->entryPath == "description.xml");
  state.RestorePrimary();
  assert(state.Displayed()->entryPath == "description.xml");
  state.SetPrimary({"GeneralSceneDescription.xml", "<mvr/>",
                    InspectorSourceSyntax::Xml});
  assert(state.Displayed()->entryPath == "GeneralSceneDescription.xml");
  state.Clear();
  assert(!state.Primary() && !state.Displayed());
  return 0;
}
