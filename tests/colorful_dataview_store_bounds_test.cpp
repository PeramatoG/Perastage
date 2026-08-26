#include "colorstore.h"

#include <cassert>

#include <wx/init.h>

// Verifies stale attribute requests cannot index beyond the live store rows.
int main() {
  wxInitializer initializer;
  if (!initializer.IsOk())
    return 0;
  ColorfulDataViewListStore store;
  assert(store.GetColumnCount() == 0);
  store.AppendColumn("bool");
  assert(store.GetColumnCount() == 1);
  for (int column = 1; column < 8; ++column)
    store.AppendColumn("string");
  assert(store.GetColumnCount() == 8);
  assert(store.GetColumnType(0) == "bool");
  assert(store.GetColumnType(7) == "string");
  assert(store.GetColumnType(8).empty());
  wxVector<wxVariant> values;
  values.push_back(true);
  for (int column = 1; column < 8; ++column)
    values.push_back(wxString::Format("cell-%d", column));
  store.AppendItem(values, 1);
  for (unsigned column = 0; column < 8; ++column) {
    wxVariant value;
    store.GetValueByRow(value, 0, column);
    assert(!value.IsNull());
  }
  wxVector<wxVariant> secondValues(values);
  secondValues[1] = wxString("second row");
  store.AppendItem(secondValues, 2);
  const wxDataViewItem firstItem = store.GetItem(0);
  const wxDataViewItem secondItem = store.GetItem(1);
  for (unsigned column = 0; column < 8; ++column)
    (void)store.Compare(firstItem, secondItem, column, true);
  (void)store.Compare(firstItem, secondItem, 8, true);
  wxVariant invalidValue;
  store.GetValue(invalidValue, firstItem, 8);
  assert(invalidValue.IsNull());
  store.DeleteItem(1);
  (void)store.Compare(firstItem, secondItem, 1, true);
  wxDataViewItemAttr attr;
  assert(!store.GetAttrByRow(1, 0, attr));
  store.DeleteAllItems();
  assert(!store.GetAttrByRow(0, 0, attr));
  return 0;
}
