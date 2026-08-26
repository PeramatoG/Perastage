#include "rider_fixture_resolution_model.h"

#include <cassert>

namespace GdtfDictionary {

// Supplies the dictionary lookup contract required by the isolated model test.
std::optional<Entry> FindInLoadedDictionary(
    const std::unordered_map<std::string, Entry> &, const std::string &, bool) {
  return std::nullopt;
}

} // namespace GdtfDictionary

// Verifies the fixed resolver schema and its direct analysis-backed values.
int main() {
  using namespace rider_fixture_resolution;
  Analysis analysis;
  Item item;
  item.request = {"GLP JDC1", "GLPJDC1", 4, {"LX1", "LX2"}};
  item.originalFixtureType = "GLP JDC1";
  item.effectiveFixtureType = "GLP JDC1";
  item.selectedEntry = mvr::gdtf_catalog_matcher::GdtfCatalogEntry{
      "rid-jdc1", "GLP", "JDC1", {{"Standard", 62}}, 1, 5.0f};
  item.selectedMode = "Standard";
  item.state = State::Suggested;
  item.origin = ResolutionOrigin::AutomaticMatch;
  item.details = "Selected by the shared GDTF catalog matcher";
  analysis.items.push_back(item);
  item.effectiveFixtureType = "Unknown fixture";
  item.request.quantity = 1;
  item.request.positions = {"FLOOR"};
  item.selectedEntry.reset();
  item.selectedMode.clear();
  item.state = State::Generic;
  item.origin = ResolutionOrigin::GenericFallback;
  item.details = "Generic fallback selected for this import";
  analysis.items.push_back(item);

  RiderFixtureResolutionModel model(analysis);
  assert(model.GetColumnCount() == 8);
  assert(model.GetColumnType(RiderFixtureResolutionModel::Create) == "bool");
  for (unsigned column = RiderFixtureResolutionModel::FixtureType;
       column < RiderFixtureResolutionModel::ColumnCount; ++column)
    assert(model.GetColumnType(column) == "string");
  assert(model.GetColumnType(RiderFixtureResolutionModel::ColumnCount).empty());

  const wxString expected[] = {"", "GLP JDC1", "4", "LX1, LX2",
                               "GLP / JDC1", "Standard", "Automatic match",
                               "Selected by the shared GDTF catalog matcher"};
  for (unsigned column = 0;
       column < RiderFixtureResolutionModel::ColumnCount; ++column) {
    wxVariant value;
    model.GetValueByRow(value, 0, column);
    assert(!value.IsNull());
    if (column == RiderFixtureResolutionModel::Create)
      assert(value.GetBool());
    else
      assert(value.GetString() == expected[column]);
  }

  wxVariant invalid;
  model.GetValueByRow(invalid, 0, RiderFixtureResolutionModel::ColumnCount);
  assert(invalid.IsNull());
  assert(!model.SetValueByRow(wxString("ignored"), 0,
                              RiderFixtureResolutionModel::Status));
  assert(model.SetValueByRow(false, 0, RiderFixtureResolutionModel::Create));
  assert(model.SetValueByRow(wxString("GLP JDC-1"), 0,
                             RiderFixtureResolutionModel::FixtureType));
  assert(!analysis.items[0].create);
  assert(analysis.items[0].effectiveFixtureType == "GLP JDC-1");
  model.NotifyRowChanged(0);

  wxDataViewItemAttr attr;
  assert(model.GetAttrByRow(0, RiderFixtureResolutionModel::Status, attr));
  assert(!model.GetAttrByRow(0, RiderFixtureResolutionModel::Mode, attr));
  for (unsigned column = 0;
       column < RiderFixtureResolutionModel::ColumnCount; ++column) {
    const int compared = model.Compare(model.GetItem(0), model.GetItem(1),
                                       column, true);
    assert(compared != 0);
  }
  assert(model.Compare(model.GetItem(0), model.GetItem(1),
                       RiderFixtureResolutionModel::ColumnCount, true) == 0);
  return 0;
}
