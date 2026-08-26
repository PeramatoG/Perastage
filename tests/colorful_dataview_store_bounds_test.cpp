#include "colorstore.h"

#include <cassert>

#include <wx/init.h>

// Verifies stale attribute requests cannot index beyond the live store rows.
int main() {
  wxInitializer initializer;
  if (!initializer.IsOk())
    return 0;
  ColorfulDataViewListStore store;
  store.AppendColumn("bool");
  for (int column = 1; column < 8; ++column)
    store.AppendColumn("string");
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
  wxDataViewItemAttr attr;
  assert(!store.GetAttrByRow(1, 0, attr));
  store.DeleteAllItems();
  assert(!store.GetAttrByRow(0, 0, attr));
  return 0;
}
