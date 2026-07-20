#include "LayoutSelectionPolicy.h"

#include <cassert>
#include <optional>
#include <string>
#include <vector>

namespace {

// Verifies the selection policy preserves an existing current layout.
void TestCurrentLayoutStillExists() {
  const auto row = layouts::ChooseLayoutSelectionRow({{"A", "B", "C"}, "B", -1});
  assert(row.has_value() && *row == 1);
}

// Verifies the selection policy chooses a nearby row after deletion.
void TestDeletedLayoutUsesNearbyRow() {
  const auto row = layouts::ChooseLayoutSelectionRow({{"A", "C"}, "B", 1});
  assert(row.has_value() && *row == 1);
}

// Verifies the selection policy falls back to the first layout without context.
void TestNoCurrentLayoutUsesFirstRow() {
  const auto row = layouts::ChooseLayoutSelectionRow({{"A", "B"}, {}, -1});
  assert(row.has_value() && *row == 0);
}

// Verifies the selection policy handles a single available layout.
void TestSingleLayout() {
  const auto row = layouts::ChooseLayoutSelectionRow({{"Only"}, {}, 5});
  assert(row.has_value() && *row == 0);
}

// Verifies the selection policy reports no selection for an empty collection.
void TestEmptyCollection() {
  const auto row = layouts::ChooseLayoutSelectionRow({{}, {}, -1});
  assert(!row.has_value());
}

} // namespace

// Runs layout selection policy coverage.
int main() {
  TestCurrentLayoutStillExists();
  TestDeletedLayoutUsesNearbyRow();
  TestNoCurrentLayoutUsesFirstRow();
  TestSingleLayout();
  TestEmptyCollection();
  return 0;
}
