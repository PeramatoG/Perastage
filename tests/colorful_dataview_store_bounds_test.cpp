#include "colorstore.h"

#include <cassert>

#include <wx/init.h>

// Verifies stale attribute requests cannot index beyond the live store rows.
int main() {
  wxInitializer initializer;
  if (!initializer.IsOk())
    return 0;
  ColorfulDataViewListStore store;
  wxVector<wxVariant> values;
  values.push_back(wxString("row"));
  store.AppendItem(values, 1);
  wxDataViewItemAttr attr;
  assert(!store.GetAttrByRow(1, 0, attr));
  store.DeleteAllItems();
  assert(!store.GetAttrByRow(0, 0, attr));
  return 0;
}
