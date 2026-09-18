#include "colorstore.h"
#include "dataview_sort_profiles.h"

#include <cassert>

#include <wx/init.h>

// Appends a one-column row with stable item data and returns its item.
static wxDataViewItem AppendValue(ColorfulDataViewListStore &store,
                                  const wxVariant &value, wxUIntPtr key) {
  wxVector<wxVariant> values;
  values.push_back(value);
  store.AppendItem(values, key);
  return store.GetItem(store.GetItemCount() - 1);
}

// Verifies focused semantic comparator behavior in both directions.
static void TestSemanticComparisons() {
  using Mode = DataViewSort::Mode;
  assert(DataViewSort::Compare(wxVariant(2L), wxVariant(10L), Mode::Text) < 0);
  assert(DataViewSort::Compare(wxVariant(10L), wxVariant(100L), Mode::Text) <
         0);
  assert(DataViewSort::Compare(wxVariant(100L), wxVariant(10L), Mode::Text) >
         0);
  assert(DataViewSort::Compare(wxVariant(wxString("6.8")),
                               wxVariant(wxString("11.8")), Mode::Decimal) < 0);
  assert(DataViewSort::Compare(wxVariant(wxString("-10.0")),
                               wxVariant(wxString("-2.0")), Mode::Decimal) < 0);
  assert(DataViewSort::Compare(wxVariant(wxString("11,8")),
                               wxVariant(wxString("6,8")), Mode::Decimal) > 0);
  assert(DataViewSort::Compare(wxVariant(wxString::FromUTF8("-10.0\xC2\xB0")),
                               wxVariant(wxString::FromUTF8("2.0\xC2\xB0")),
                               Mode::Decimal) < 0);
  assert(DataViewSort::Compare(wxVariant(wxString::FromUTF8("2.0\xC2\xB0")),
                               wxVariant(wxString::FromUTF8("12.0\xC2\xB0")),
                               Mode::Decimal) < 0);
  assert(DataViewSort::Compare(wxVariant(wxString("LX2")),
                               wxVariant(wxString("LX10")),
                               Mode::NaturalText) < 0);
  assert(DataViewSort::Compare(wxVariant(wxString("LX1")),
                               wxVariant(wxString("LX2")),
                               Mode::NaturalText) < 0);
  assert(DataViewSort::Compare(wxVariant(wxString("Fixture 2")),
                               wxVariant(wxString("Fixture 10")),
                               Mode::NaturalText) < 0);
  assert(DataViewSort::Compare(wxVariant(wxString("")),
                               wxVariant(wxString("invalid")),
                               Mode::Decimal) < 0);
  assert(DataViewSort::Compare(wxVariant(wxString("invalid")),
                               wxVariant(wxString("6.8")), Mode::Decimal) > 0);
  assert(DataViewSort::Compare(wxVariant(false), wxVariant(true),
                               Mode::Boolean) < 0);

  ColorfulDataViewListStore store;
  store.AppendColumn("string");
  const wxDataViewItem ten = AppendValue(store, wxVariant(wxString("10")), 2);
  const wxDataViewItem two = AppendValue(store, wxVariant(wxString("2")), 1);
  assert(store.Compare(ten, two, 0, true) < 0);
  store.SetColumnSortMode(0, Mode::Integer);
  assert(store.Compare(two, ten, 0, true) < 0);
  assert(store.Compare(two, ten, 0, false) > 0);

  ColorfulDataViewListStore tieStore;
  tieStore.AppendColumn("string");
  tieStore.SetColumnSortMode(0, Mode::Decimal);
  const wxDataViewItem later =
      AppendValue(tieStore, wxVariant(wxString("6.80")), 20);
  const wxDataViewItem earlier =
      AppendValue(tieStore, wxVariant(wxString("6,8")), 10);
  assert(tieStore.Compare(earlier, later, 0, true) < 0);
}

// Verifies named table profiles register the requested independent policies.
static void TestSortProfiles() {
  using Mode = DataViewSort::Mode;
  ColorfulDataViewListStore fixture;
  DataViewSortProfiles::ConfigureFixture(fixture);
  assert(fixture.GetColumnSortMode(FixtureTableColumns::ToIndex(
             FixtureTableColumns::Column::FixtureId)) == Mode::Integer);
  assert(fixture.GetColumnSortMode(FixtureTableColumns::ToIndex(
             FixtureTableColumns::Column::Universe)) == Mode::Integer);
  assert(fixture.GetColumnSortMode(FixtureTableColumns::ToIndex(
             FixtureTableColumns::Column::Channel)) == Mode::Integer);
  assert(fixture.GetColumnSortMode(FixtureTableColumns::ToIndex(
             FixtureTableColumns::Column::Weight)) == Mode::Decimal);
  ColorfulDataViewListStore truss;
  DataViewSortProfiles::ConfigureTruss(truss);
  assert(truss.GetColumnSortMode(TableColumnIndices::ToIndex(
             TrussTableColumns::Column::Weight)) == Mode::Decimal);
  ColorfulDataViewListStore hoist;
  DataViewSortProfiles::ConfigureHoist(hoist);
  assert(hoist.GetColumnSortMode(TableColumnIndices::ToIndex(
             HoistTableColumns::Column::HoistId)) == Mode::Integer);
  assert(hoist.GetColumnSortMode(TableColumnIndices::ToIndex(
             HoistTableColumns::Column::Load)) == Mode::Decimal);
  ColorfulDataViewListStore sceneObject;
  DataViewSortProfiles::ConfigureSceneObject(sceneObject);
  assert(sceneObject.GetColumnSortMode(TableColumnIndices::ToIndex(
             SceneObjectTableColumns::Column::PositionX)) == Mode::Decimal);
  assert(sceneObject.GetColumnSortMode(TableColumnIndices::ToIndex(
             SceneObjectTableColumns::Column::Name)) == Mode::NaturalText);
}

// Verifies stale attribute requests cannot index beyond the live store rows.
int main() {
  wxInitializer initializer;
  if (!initializer.IsOk())
    return 0;
  TestSemanticComparisons();
  TestSortProfiles();
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
