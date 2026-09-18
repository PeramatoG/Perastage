#include "hoisttable/hoist_table_edit_service.h"

#include "matrixutils.h"
#include "table_column_indices.h"

#include <cassert>
#include <cmath>
#include <memory>
#include <wx/app.h>
#include <wx/dataview.h>
#include <wx/frame.h>

namespace {

class TestApp : public wxApp {
public:
  // Initializes the GUI runtime required by the data-view control.
  bool OnInit() override { return true; }
};

wxIMPLEMENT_APP_NO_MAIN(TestApp);

class AppScope {
public:
  // Starts a minimal GUI-capable wxWidgets application.
  AppScope() {
    int argc = 0;
    char **argv = nullptr;
    started = wxEntryStart(argc, argv);
    if (started && wxTheApp)
      initialized = wxTheApp->CallOnInit();
  }
  // Stops the test application after all widgets have been destroyed.
  ~AppScope() {
    if (started)
      wxEntryCleanup();
  }
  // Reports whether controls can be created in this environment.
  bool IsOk() const { return started && initialized; }

private:
  bool started = false;
  bool initialized = false;
};

class Adapter final : public HoistTableEditService::ISceneAdapter {
public:
  // Counts undo snapshots requested by an effective edit operation.
  void PushUndoState(const std::string &description) override {
    assert(description == "edit support");
    ++undoCount;
  }
  // Returns the scene under test.
  MvrScene &GetScene() override { return scene; }
  // Uses metric distance cells in the test table.
  Units::DistanceUnitSystem GetDistanceUnitSystem() const override {
    return Units::DistanceUnitSystem::Metric;
  }
  // Uses kilogram weight cells in the test table.
  Units::WeightUnitSystem GetWeightUnitSystem() const override {
    return Units::WeightUnitSystem::Metric;
  }
  // Resolves the deterministic test profile by stable ID.
  std::optional<DummyHoistProfile>
  FindDummyProfileById(const std::string &id) const override {
    return id == profile.id ? std::optional(profile) : std::nullopt;
  }
  // Resolves the deterministic test profile by displayed name.
  std::optional<DummyHoistProfile>
  FindDummyProfileByDisplayName(const std::string &name) const override {
    return name == profile.displayName ? std::optional(profile) : std::nullopt;
  }

  MvrScene scene;
  int undoCount = 0;
  DummyHoistProfile profile{"profile-id", "Preset", "Inherited motor",
                            "Maker", "Model", 1000.0f, 25.0f, "Audio"};
};

using Column = HoistTableColumns::Column;

// Creates a representative support with inherited profile data.
Support MakeSupport(const std::string &uuid) {
  Support support;
  support.uuid = uuid;
  support.name = "Hoist";
  support.function = "Hoist";
  support.layer = "Rigging";
  support.positionName = "LX1";
  support.position = "position-id";
  support.dummyProfileId = "profile-id";
  support.dummyPreset = "Preset";
  support.loadKg = 100.0f;
  support.chainLength = 5.0f;
  return support;
}

// Returns a complete table row matching one scene support.
wxVector<wxVariant> MakeRow(const Support &support, const Adapter &adapter) {
  const auto effective = ResolveEffectiveSupportData(
      support, HoistPresetDefaults{adapter.profile.motorName,
                                   adapter.profile.motorManufacturer,
                                   adapter.profile.motorModel,
                                   adapter.profile.capacityKg,
                                   adapter.profile.weightKg,
                                   adapter.profile.hoistFunction});
  const auto euler = MatrixUtils::MatrixToEuler(support.transform);
  wxVector<wxVariant> row;
  row.push_back(wxVariant(1));
  row.push_back(wxString::FromUTF8(support.name));
  row.push_back(wxString::FromUTF8(support.function));
  row.push_back(wxString::FromUTF8(effective.hoistFunction));
  row.push_back(wxString::FromUTF8(effective.motorName));
  row.push_back(wxString::FromUTF8(support.dummyPreset));
  row.push_back(wxString::FromUTF8(support.layer));
  row.push_back(wxString::FromUTF8(support.positionName));
  row.push_back(wxString::Format("%.3f", support.transform.o[0] / 1000.0f));
  row.push_back(wxString::Format("%.3f", support.transform.o[1] / 1000.0f));
  row.push_back(wxString::Format("%.3f", support.transform.o[2] / 1000.0f));
  row.push_back(wxString::Format("%.1f", euler[2]));
  row.push_back(wxString::Format("%.1f", euler[1]));
  row.push_back(wxString::Format("%.1f", euler[0]));
  row.push_back(wxString::Format("%.2f", support.chainLength));
  row.push_back(wxString::Format("%.3f", effective.capacityKg));
  row.push_back(wxString::Format("%.3f", effective.weightKg));
  row.push_back(wxString::Format("%.3f", support.loadKg));
  return row;
}

// Creates a table with the stable hoist model columns.
std::unique_ptr<wxDataViewListCtrl> MakeTable(wxWindow *parent) {
  auto table = std::make_unique<wxDataViewListCtrl>(parent, wxID_ANY);
  for (size_t column = 0; column < TableColumnIndices::Count<Column>(); ++column)
    table->AppendTextColumn(wxString::Format("%zu", column));
  return table;
}

// Updates one table cell through its named column.
void Set(wxDataViewListCtrl &table, unsigned row, Column column,
         const wxString &value) {
  table.SetValue(wxVariant(value), row, TableColumnIndices::ToIndex(column));
}

// Runs focused row-to-support mutation regression cases.
void RunCases(wxWindow *parent) {
  Adapter adapter;
  adapter.scene.supports["a"] = MakeSupport("a");
  adapter.scene.supports["other"] = MakeSupport("other");
  adapter.scene.supports["other"].name = "Untouched";
  auto table = MakeTable(parent);
  table->AppendItem(MakeRow(adapter.scene.supports.at("a"), adapter));
  std::unordered_map<std::string, float> pending;

  auto result = HoistTableEditService::UpdateSceneData(
      adapter, {table.get(), {"a"}, pending});
  assert(!result.anyChanged && adapter.undoCount == 0);

  Set(*table, 0, Column::Name, "Renamed");
  Set(*table, 0, Column::Type, "Motor");
  Set(*table, 0, Column::Layer, "New layer");
  Set(*table, 0, Column::HangPosition, "LX2");
  Set(*table, 0, Column::Motor, "Custom motor");
  Set(*table, 0, Column::Capacity, "1200");
  Set(*table, 0, Column::Weight, "30");
  Set(*table, 0, Column::Function, "Video");
  Set(*table, 0, Column::PositionX, "1.250");
  adapter.scene.supports.at("a").transform.u = {2.0f, 0.0f, 0.0f};
  adapter.scene.supports.at("a").transform.v = {0.0f, 3.0f, 0.0f};
  adapter.scene.supports.at("a").transform.w = {0.0f, 0.0f, 4.0f};
  result = HoistTableEditService::UpdateSceneData(
      adapter, {table.get(), {"a"}, pending});
  const Support &edited = adapter.scene.supports.at("a");
  assert(result.anyChanged && adapter.undoCount == 1);
  assert(edited.name == "Renamed" && edited.function == "Motor");
  assert(edited.layer == "New layer" && edited.positionName == "LX2");
  assert(edited.motorNameSource == "Manual" && edited.capacitySource == "Manual");
  assert(edited.weightSource == "Manual" && edited.hoistFunctionSource == "Manual");
  assert(result.changedWeightPositions.contains("LX1"));
  assert(result.changedWeightPositions.contains("LX2"));
  assert(adapter.scene.positions.at("position-id") == "LX2");
  assert(std::abs(edited.transform.o[0] - 1250.0f) < 0.01f);
  const auto scale = MatrixUtils::ExtractScale(edited.transform);
  assert(std::abs(scale[0] - 2.0f) < 0.001f);
  assert(std::abs(scale[1] - 3.0f) < 0.001f);
  assert(std::abs(scale[2] - 4.0f) < 0.001f);
  assert(adapter.scene.supports.at("other").name == "Untouched");

  Set(*table, 0, Column::DummyPreset, "Unknown");
  result = HoistTableEditService::UpdateSceneData(
      adapter, {table.get(), {"a"}, pending});
  assert(adapter.scene.supports.at("a").dummyProfileId.empty());
  Set(*table, 0, Column::DummyPreset, "Preset");
  result = HoistTableEditService::UpdateSceneData(
      adapter, {table.get(), {"a"}, pending});
  assert(adapter.scene.supports.at("a").dummyProfileId == "profile-id");
  Set(*table, 0, Column::DummyPreset, "");
  result = HoistTableEditService::UpdateSceneData(
      adapter, {table.get(), {"a"}, pending});
  assert(adapter.scene.supports.at("a").dummyProfileId.empty());

  pending["a"] = 150.0f;
  Set(*table, 0, Column::Load, "150");
  result = HoistTableEditService::UpdateSceneData(
      adapter, {table.get(), {"a"}, pending});
  assert(adapter.scene.supports.at("a").loadSource == "Auto");
  Set(*table, 0, Column::Load, "140");
  result = HoistTableEditService::UpdateSceneData(
      adapter, {table.get(), {"a"}, pending});
  assert(adapter.scene.supports.at("a").loadSource == "Manual");

  adapter.scene.supports["b"] = MakeSupport("b");
  table->AppendItem(MakeRow(adapter.scene.supports.at("b"), adapter));
  Set(*table, 0, Column::Name, "First");
  Set(*table, 1, Column::Name, "Second");
  const int undoBefore = adapter.undoCount;
  result = HoistTableEditService::UpdateSceneData(
      adapter, {table.get(), {"a", "b"}, pending});
  assert(result.anyChanged && adapter.undoCount == undoBefore + 1);
}

} // namespace

// Executes the service tests when a GUI runtime is available.
int main() {
  AppScope app;
  if (!app.IsOk())
    return 77;
  auto frame = std::make_unique<wxFrame>(nullptr, wxID_ANY, "test");
  RunCases(frame.get());
  frame.reset();
  return 0;
}
