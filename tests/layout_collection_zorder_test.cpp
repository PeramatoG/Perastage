#include "LayoutCollection.h"

#include <cassert>

namespace {

layouts::LayoutDefinition BuildLayout() {
  layouts::LayoutDefinition layout;
  layout.name = "Z order";

  layouts::Layout2DViewDefinition view;
  view.id = 1;
  view.zIndex = 1;
  layout.view2dViews.push_back(view);

  layouts::LayoutLegendDefinition legend;
  legend.id = 2;
  legend.zIndex = 5;
  layout.legendViews.push_back(legend);

  layouts::LayoutTextDefinition text;
  text.id = 3;
  text.zIndex = -3;
  layout.textViews.push_back(text);

  return layout;
}

const layouts::LayoutDefinition *FindLayout(const layouts::LayoutCollection &collection,
                                            const char *name) {
  for (const auto &layout : collection.Items()) {
    if (layout.name == name)
      return &layout;
  }
  return nullptr;
}

void TestMoveUpdatesZIndexForCrossElementOrdering() {
  layouts::LayoutCollection collection;
  assert(collection.AddLayout(BuildLayout()));

  assert(collection.MoveLayout2DView("Z order", 1, true));
  assert(collection.MoveLayoutLegend("Z order", 2, false));

  const layouts::LayoutDefinition *layout = FindLayout(collection, "Z order");
  assert(layout != nullptr);
  assert(layout->view2dViews.size() == 1);
  assert(layout->legendViews.size() == 1);

  // Bringing the view to front should place it above every element.
  assert(layout->view2dViews.front().zIndex == 6);
  // Sending the legend to back should place it below every element.
  assert(layout->legendViews.front().zIndex == -4);

  // Re-applying move to front/back should be stable when already at boundary.
  assert(collection.MoveLayout2DView("Z order", 1, true));
  assert(collection.MoveLayoutLegend("Z order", 2, false));
  layout = FindLayout(collection, "Z order");
  assert(layout != nullptr);
  assert(layout->view2dViews.front().zIndex == 6);
  assert(layout->legendViews.front().zIndex == -4);
}

void TestMoveEventTableUpdatesZIndex() {
  layouts::LayoutCollection collection;
  layouts::LayoutDefinition layout;
  layout.name = "Events";

  layouts::LayoutEventTableDefinition table;
  table.id = 9;
  table.zIndex = 2;
  layout.eventTables.push_back(table);

  layouts::LayoutImageDefinition image;
  image.id = 4;
  image.zIndex = 10;
  layout.imageViews.push_back(image);

  assert(collection.AddLayout(layout));
  assert(collection.MoveLayoutEventTable("Events", 9, true));

  const layouts::LayoutDefinition *updated = FindLayout(collection, "Events");
  assert(updated != nullptr);
  assert(updated->eventTables.front().zIndex == 11);
}

} // namespace

int main() {
  TestMoveUpdatesZIndexForCrossElementOrdering();
  TestMoveEventTableUpdatesZIndex();
  return 0;
}
