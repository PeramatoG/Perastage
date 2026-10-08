#include "gdtfsearchdialog.h"
#include "gdtf_catalog_details_panel.h"
#include "ui_feature_flags.h"
#include <wx/app.h>
#include <wx/splitter.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

// This test exercises the catalog UI without linking unrelated print policy.
namespace ui {
bool IsFeatureEnabled(FeatureFlag) { return false; }
}

class CatalogTestApp final : public wxApp {
public:
  bool OnInit() override { return true; }
};
wxIMPLEMENT_APP_NO_MAIN(CatalogTestApp);

namespace {
using Clock = std::chrono::steady_clock;
const std::string payload = R"({"list":[{"rid":"selected","manufacturer":"Martin",
  "fixture":"VDO Sceptron","revision":"Tour","uuid":"abc-123","filesize":2048,
  "modes":[{"name":"Extended","dmxfootprint":36}]}]})";

template <class Predicate> void PumpUntil(Predicate predicate) {
  const auto deadline = Clock::now() + std::chrono::seconds(5);
  while (!predicate() && Clock::now() < deadline) {
    wxTheApp->Yield(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  assert(predicate());
}

template <class T> T *FindChild(wxWindow &parent) {
  for (auto *child : parent.GetChildren()) {
    if (auto *match = dynamic_cast<T *>(child))
      return match;
    if (auto *nested = FindChild<T>(*child))
      return nested;
  }
  return nullptr;
}

void SelectFirst(GdtfSearchDialog &dialog, wxDataViewListCtrl &table) {
  table.Select(table.RowToItem(0));
  wxDataViewEvent selected(wxEVT_DATAVIEW_SELECTION_CHANGED, &table,
                            table.RowToItem(0));
  table.GetEventHandler()->ProcessEvent(selected);
  assert(dialog.GetSelectedId() == "selected");
  auto *details = FindChild<GdtfCatalogDetailsPanel>(dialog);
  assert(details);
  auto *modes = FindChild<wxDataViewListCtrl>(*details);
  assert(modes && modes->GetColumnCount() == 2 && modes->GetItemCount() == 1);
  assert(modes->GetTextValue(0, 0) == "Extended" && modes->GetTextValue(0, 1) == "36");
  bool uuidFound = false, sizeFound = false;
  for (auto *child : details->GetChildren()) {
    if (auto *value = dynamic_cast<wxTextCtrl *>(child)) {
      if (value->GetValue() == "abc-123") {
        uuidFound = true;
        assert(value->HasFlag(wxTE_READONLY));
      }
    }
    if (auto *value = dynamic_cast<wxStaticText *>(child))
      sizeFound |= value->GetLabel() == "2.0 KiB";
  }
  assert(uuidFound && sizeFound);
}

void RunRefresh(bool changed, bool fail) {
  auto parsed = mvr::gdtf_catalog_parser::ParseCatalog(payload);
  auto refreshed = parsed;
  refreshed.payloadFingerprint.clear();
  if (changed) {
    for (int i = 0; i < 501; ++i) {
      auto row = parsed.entries.front();
      row.rid = "new-" + std::to_string(i);
      refreshed.entries.insert(refreshed.entries.begin(), row);
    }
    refreshed.entries.back().revision = "Changed";
  }
  std::atomic<bool> entered{false}, release{false};
  GdtfSearchDialog dialog(nullptr, payload, "2026-10-08 10:00:00", [&] {
    entered = true;
    while (!release)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (fail)
      throw std::runtime_error("Simulated offline refresh");
    GdtfSearchDialog::RefreshResult result;
    result.success = true;
    // A supplied parse must be reused even when raw data cannot be parsed.
    result.listData = "already-parsed-refresh";
    result.parsedCatalog = refreshed;
    result.updatedAt = "2026-10-08 11:00:00";
    return result;
  }, GdtfCatalogDisplaySource::Cached, true, {}, parsed.entries);
  auto *table = FindChild<wxDataViewListCtrl>(dialog);
  assert(table && table->GetColumnCount() == 6 && table->GetItemCount() == 1);
  auto *download = dialog.FindWindow(wxID_OK);
  auto *query = FindChild<wxTextCtrl>(dialog);
  assert(download && query);
  SelectFirst(dialog, *table);
  dialog.Show();
  PumpUntil([&] { return entered.load(); });
  assert(!download->IsEnabled());
  query->SetValue("sceptron martin");
  wxCommandEvent search(wxEVT_TEXT_ENTER, query->GetId());
  query->GetEventHandler()->ProcessEvent(search);
  assert(table->GetItemCount() == 1 && dialog.GetSelectedId() == "selected");
  assert(!download->IsEnabled());
  table->SetTextValue("render-marker", 0, 2);
  release = true;
  PumpUntil([&] { return download->IsEnabled(); });
  assert(dialog.GetSelectedId() == "selected");
  if (changed && !fail) {
    assert(dialog.GetSelectedEntry()->revision == "Changed");
    assert(table->GetItemCount() == 2); // Selection follows its RID to page 2.
    assert(dialog.GetCurrentListData() == "already-parsed-refresh");
  } else {
    assert(table->GetItemCount() == 1 && dialog.GetCurrentListData() == payload);
    assert(table->GetTextValue(0, 2) == "render-marker");
  }
  dialog.Hide();
}
} // namespace

int main() {
  int argc = 0;
  char **argv = nullptr;
  if (!wxEntryStart(argc, argv))
    return 77;
  assert(wxTheApp->CallOnInit());
  {
    // No callback means a fresh-cache opening cannot start online work.
    auto parsed = mvr::gdtf_catalog_parser::ParseCatalog(payload);
    GdtfSearchDialog fresh(nullptr, "already-parsed-cache", {}, nullptr,
                            GdtfCatalogDisplaySource::Cached, true, {}, parsed.entries);
    auto *table = FindChild<wxDataViewListCtrl>(fresh);
    assert(table && table->GetItemCount() == 1);
    SelectFirst(fresh, *table);
    fresh.Show();
    wxTheApp->Yield(true);
    assert(fresh.FindWindow(wxID_OK)->IsEnabled());
    fresh.Hide();
  }
  RunRefresh(false, false);
  RunRefresh(true, false);
  RunRefresh(false, true);
  {
    // Destruction joins the worker before its event target can disappear.
    std::atomic<bool> started{false}, finished{false};
    auto parsed = mvr::gdtf_catalog_parser::ParseCatalog(payload);
    auto dialog = std::make_unique<GdtfSearchDialog>(nullptr, payload, "", [&] {
      started = true;
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
      finished = true;
      return GdtfSearchDialog::RefreshResult{};
    }, GdtfCatalogDisplaySource::Cached, true, "", parsed.entries);
    dialog->Show();
    PumpUntil([&] { return started.load(); });
    dialog.reset();
    assert(finished.load());
    wxTheApp->Yield(true);
  }
  wxEntryCleanup();
}
