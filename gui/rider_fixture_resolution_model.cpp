#include "rider_fixture_resolution_model.h"

#include "gdtf_resolution_status_style.h"

#include <filesystem>

#include <wx/translation.h>

namespace {

// Joins position labels for compact table presentation.
wxString JoinPositions(const std::vector<std::string> &positions) {
  wxString value;
  for (const std::string &position : positions) {
    if (!value.empty())
      value += ", ";
    value += wxString::FromUTF8(position);
  }
  return value;
}

// Formats the selected or suggested catalog identity for a resolution row.
wxString FormatCatalogIdentity(const rider_fixture_resolution::Item &item) {
  if (item.state == rider_fixture_resolution::State::Dictionary &&
      item.dictionaryEntry && !item.dictionaryEntry->path.empty()) {
    return wxString::FromUTF8(
        std::filesystem::path(item.dictionaryEntry->path).filename().string());
  }
  if (item.state == rider_fixture_resolution::State::Generic ||
      (!item.selectedEntry && !item.suggestedEntry))
    return _("Generic fallback");
  if (!item.selectedEntry && item.suggestedEntry) {
    const auto &suggested = *item.suggestedEntry;
    return wxString::FromUTF8(
        "Generic fallback (suggested: " + suggested.manufacturer + " / " +
        suggested.fixtureName + ")");
  }
  const auto &entry = item.selectedEntry ? item.selectedEntry : item.suggestedEntry;
  if (!entry)
    return _("-");
  if (entry->manufacturer.empty())
    return wxString::FromUTF8(entry->fixtureName);
  return wxString::FromUTF8(entry->manufacturer + " / " + entry->fixtureName);
}

} // namespace

// Creates a fixed-schema view over the authoritative resolution analysis.
RiderFixtureResolutionModel::RiderFixtureResolutionModel(
    rider_fixture_resolution::Analysis &analysisIn)
    : wxDataViewIndexListModel(
          static_cast<unsigned int>(analysisIn.items.size())),
      analysis(analysisIn) {}

// Returns the resolver's compile-time model column count.
unsigned int RiderFixtureResolutionModel::GetColumnCount() const {
  return ColumnCount;
}

// Returns the renderer-compatible variant type for a model column.
wxString RiderFixtureResolutionModel::GetColumnType(unsigned int column) const {
  if (column >= ColumnCount)
    return wxString();
  return column == Create ? wxS("bool") : wxS("string");
}

// Exposes one analysis field without maintaining a duplicate variant row.
void RiderFixtureResolutionModel::GetValueByRow(wxVariant &value,
                                                 unsigned int row,
                                                 unsigned int column) const {
  if (row >= analysis.items.size() || column >= ColumnCount) {
    value.MakeNull();
    return;
  }
  const auto &item = analysis.items[row];
  switch (column) {
  case Create: value = item.create; break;
  case FixtureType: value = wxString::FromUTF8(item.effectiveFixtureType); break;
  case Quantity: value = wxString::Format("%d", item.request.quantity); break;
  case Positions: value = JoinPositions(item.request.positions); break;
  case SelectedGdtf: value = FormatCatalogIdentity(item); break;
  case Mode:
    value = item.selectedMode.empty() ? wxString("-")
                                      : wxString::FromUTF8(item.selectedMode);
    break;
  case Status:
    value = wxString::FromUTF8(rider_fixture_resolution::OriginName(item.origin));
    break;
  case Details: value = wxString::FromUTF8(item.details); break;
  default: value.MakeNull(); break;
  }
}

// Commits only the two directly editable table fields to the analysis.
bool RiderFixtureResolutionModel::SetValueByRow(const wxVariant &value,
                                                 unsigned int row,
                                                 unsigned int column) {
  if (row >= analysis.items.size() || column >= ColumnCount)
    return false;
  auto &item = analysis.items[row];
  if (column == Create) {
    item.create = value.GetBool();
    return true;
  }
  if (column == FixtureType) {
    item.effectiveFixtureType = value.GetString().ToStdString();
    return true;
  }
  return false;
}

// Applies semantic coloring only to the authoritative Status cell.
bool RiderFixtureResolutionModel::GetAttrByRow(unsigned int row,
                                                unsigned int column,
                                                wxDataViewItemAttr &attr) const {
  if (row >= analysis.items.size() || column != Status)
    return false;
  const auto semantic = rider_fixture_resolution::StatusSemanticForOrigin(
      analysis.items[row].origin);
  if (semantic == rider_fixture_resolution::StatusSemantic::Neutral)
    return false;
  attr.SetColour(GdtfResolutionStatusColour(semantic));
  return true;
}

// Sorts valid model values while preserving analysis-index identity as a tie-break.
int RiderFixtureResolutionModel::Compare(const wxDataViewItem &item1,
                                          const wxDataViewItem &item2,
                                          unsigned int column,
                                          bool ascending) const {
  if (!item1.IsOk() || !item2.IsOk() || column >= ColumnCount)
    return 0;
  const unsigned row1 = GetRow(item1);
  const unsigned row2 = GetRow(item2);
  if (row1 >= analysis.items.size() || row2 >= analysis.items.size())
    return 0;
  wxVariant value1;
  wxVariant value2;
  GetValueByRow(value1, row1, column);
  GetValueByRow(value2, row2, column);
  int result = 0;
  if (column == Create) {
    result = static_cast<int>(value1.GetBool()) -
             static_cast<int>(value2.GetBool());
  } else {
    result = value1.GetString().CmpNoCase(value2.GetString());
  }
  if (result == 0)
    result = row1 < row2 ? -1 : row1 > row2 ? 1 : 0;
  return ascending ? result : -result;
}

// Notifies the view that one stable analysis row changed in place.
void RiderFixtureResolutionModel::NotifyRowChanged(size_t row) {
  if (row < analysis.items.size())
    RowChanged(static_cast<unsigned int>(row));
}
