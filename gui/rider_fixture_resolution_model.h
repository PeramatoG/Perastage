#pragma once

#include "../core/rider_fixture_resolution.h"

#include <wx/dataview.h>

// Adapts the fixture-resolution analysis to a fixed wxDataView schema.
class RiderFixtureResolutionModel final : public wxDataViewIndexListModel {
public:
  enum Column : unsigned {
    Create = 0,
    FixtureType,
    Quantity,
    Positions,
    SelectedGdtf,
    Mode,
    Status,
    Details,
    ColumnCount
  };

  explicit RiderFixtureResolutionModel(
      rider_fixture_resolution::Analysis &analysis);

  unsigned int GetColumnCount() const override;
  wxString GetColumnType(unsigned int column) const override;
  void GetValueByRow(wxVariant &value, unsigned int row,
                     unsigned int column) const override;
  bool SetValueByRow(const wxVariant &value, unsigned int row,
                     unsigned int column) override;
  bool GetAttrByRow(unsigned int row, unsigned int column,
                    wxDataViewItemAttr &attr) const override;
  int Compare(const wxDataViewItem &item1, const wxDataViewItem &item2,
              unsigned int column, bool ascending) const override;

  void NotifyRowChanged(size_t row);

private:
  rider_fixture_resolution::Analysis &analysis;
};

static_assert(RiderFixtureResolutionModel::ColumnCount == 8);
