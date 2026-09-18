#include "trusstable/truss_table_edit_service.h"

#include "matrixutils.h"
#include "table_column_indices.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
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

class Adapter final : public TrussTableEditService::ISceneAdapter {
public:
  // Counts undo snapshots requested by an effective edit operation.
  void PushUndoState(const std::string &) override { ++undoCount; }

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

  MvrScene scene;
  int undoCount = 0;
};

using Column = TrussTableColumns::Column;

// Returns a complete table row matching one scene truss.
wxVector<wxVariant> MakeRow(const Truss &truss) {
  const auto euler = MatrixUtils::MatrixToEuler(truss.transform);
  wxVector<wxVariant> row;
  row.push_back(wxString::FromUTF8(truss.name));
  row.push_back(wxString::FromUTF8(truss.layer));
  row.push_back(wxString::FromUTF8(truss.modelFile));
  row.push_back(wxString::FromUTF8(truss.positionName));
  row.push_back(wxString::Format("%.3f", truss.transform.o[0] / 1000.0f));
  row.push_back(wxString::Format("%.3f", truss.transform.o[1] / 1000.0f));
  row.push_back(wxString::Format("%.3f", truss.transform.o[2] / 1000.0f));
  row.push_back(wxString::Format("%.1f", euler[2]));
  row.push_back(wxString::Format("%.1f", euler[1]));
  row.push_back(wxString::Format("%.1f", euler[0]));
  row.push_back(wxString::FromUTF8(truss.manufacturer));
  row.push_back(wxString::FromUTF8(truss.model));
  row.push_back(wxString::Format("%.3f", truss.lengthMm / 1000.0f));
  row.push_back(truss.widthMm > 0.0f
                    ? wxString::Format("%.3f", truss.widthMm / 1000.0f)
                    : wxString());
  row.push_back(truss.heightMm > 0.0f
                    ? wxString::Format("%.3f", truss.heightMm / 1000.0f)
                    : wxString());
  row.push_back(wxString::Format("%.3f", truss.weightKg));
  row.push_back(truss.hasManualLoadOverride
                    ? wxString::Format("%.3f", truss.manualLoadKg)
                    : wxString());
  return row;
}

// Creates a table with the stable truss model columns.
std::unique_ptr<wxDataViewListCtrl> MakeTable(wxWindow *parent) {
  auto table = std::make_unique<wxDataViewListCtrl>(parent, wxID_ANY);
  for (size_t column = 0; column < TableColumnIndices::Count<Column>(); ++column)
    table->AppendTextColumn(wxString::Format("%zu", column));
  return table;
}

// Creates a representative truss with stable physical metadata.
Truss MakeTruss(const std::string &uuid, const std::string &name = "Type") {
  Truss truss;
  truss.uuid = uuid;
  truss.name = name;
  truss.layer = "Rigging";
  truss.manufacturer = "Maker";
  truss.model = "Model";
  truss.lengthMm = 2000.0f;
  truss.widthMm = 300.0f;
  truss.heightMm = 400.0f;
  truss.weightKg = 20.0f;
  truss.positionName = "LX1";
  return truss;
}

// Runs the focused row-to-scene mutation regression cases.
void RunCases(wxWindow *parent) {
  Adapter adapter;
  adapter.scene.trusses["a"] = MakeTruss("a");
  auto table = MakeTable(parent);
  table->AppendItem(MakeRow(adapter.scene.trusses.at("a")));

  auto result = TrussTableEditService::UpdateSceneData(
      adapter, table.get(), {"a"}, {}, {});
  assert(!result.anyChanged);
  assert(adapter.undoCount == 0);

  table->SetValue(wxVariant("Renamed"), 0,
                  TableColumnIndices::ToIndex(Column::Name));
  result = TrussTableEditService::UpdateSceneData(adapter, table.get(), {"a"},
                                                   {}, {});
  assert(result.anyChanged && adapter.undoCount == 1);
  assert(adapter.scene.trusses.at("a").name == "Renamed");

  Truss &first = adapter.scene.trusses.at("a");
  first.transform.u = {2.0f, 0.0f, 0.0f};
  first.transform.v = {0.0f, 3.0f, 0.0f};
  first.transform.w = {0.0f, 0.0f, 4.0f};
  table->SetValue(wxVariant("1.250"), 0,
                  TableColumnIndices::ToIndex(Column::PositionX));
  result = TrussTableEditService::UpdateSceneData(adapter, table.get(), {"a"},
                                                   {}, {});
  assert(result.anyChanged && adapter.undoCount == 2);
  assert(std::abs(first.transform.o[0] - 1250.0f) < 0.01f);
  const auto scale = MatrixUtils::ExtractScale(first.transform);
  assert(std::abs(scale[0] - 2.0f) < 0.001f);
  assert(std::abs(scale[1] - 3.0f) < 0.001f);
  assert(std::abs(scale[2] - 4.0f) < 0.001f);

  const std::filesystem::path temp =
      std::filesystem::temp_directory_path() / "perastage-truss-edit-test";
  std::filesystem::create_directories(temp);
  std::ofstream(temp / "model.glb") << "test";
  first.modelFile = "model.glb";
  first.symbolFile = "model.glb";
  adapter.scene.basePath = temp.string();
  result = TrussTableEditService::UpdateSceneData(
      adapter, table.get(), {"a"}, {wxString((temp / "model.glb").string())},
      {wxString((temp / "model.glb").string())});
  assert(first.modelFile == "model.glb" && first.symbolFile == "model.glb");

  table->SetValue(wxVariant("12.5"), 0,
                  TableColumnIndices::ToIndex(Column::Load));
  result = TrussTableEditService::UpdateSceneData(adapter, table.get(), {"a"},
                                                   {}, {});
  assert(first.hasManualLoadOverride && first.manualLoadKg == 12.5f);
  table->SetValue(wxVariant(""), 0,
                  TableColumnIndices::ToIndex(Column::Load));
  result = TrussTableEditService::UpdateSceneData(adapter, table.get(), {"a"},
                                                   {}, {});
  assert(!first.hasManualLoadOverride && first.manualLoadKg == 0.0f);

  adapter.scene.trusses["b"] = MakeTruss("b", first.name);
  adapter.scene.trusses["b"].lengthMm = 1000.0f;
  adapter.scene.trusses["b"].weightKg = 10.0f;
  adapter.scene.trusses["c"] = MakeTruss("c", "Other");
  adapter.scene.trusses["c"].weightKg = 30.0f;
  table->AppendItem(MakeRow(adapter.scene.trusses.at("b")));
  table->AppendItem(MakeRow(adapter.scene.trusses.at("c")));
  table->SetValue(wxVariant("25.0"), 0,
                  TableColumnIndices::ToIndex(Column::Weight));
  table->SetValue(wxVariant("LX2"), 0,
                  TableColumnIndices::ToIndex(Column::HangPosition));
  const int undoBeforeMultiRow = adapter.undoCount;
  result = TrussTableEditService::UpdateSceneData(
      adapter, table.get(), {"a", "b", "c"}, {}, {});
  assert(result.anyChanged && adapter.undoCount == undoBeforeMultiRow + 1);
  assert(result.changedWeightPositions.contains("LX1"));
  assert(result.changedWeightPositions.contains("LX2"));
  assert(adapter.scene.trusses.at("b").lengthMm == first.lengthMm);
  assert(adapter.scene.trusses.at("b").weightKg == first.weightKg);
  assert(adapter.scene.trusses.at("c").weightKg == 30.0f);
  wxVariant synchronizedLength;
  table->GetValue(synchronizedLength, 1,
                  TableColumnIndices::ToIndex(Column::Length));
  assert(synchronizedLength.GetString() == "2.00");

  std::filesystem::remove_all(temp);
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
