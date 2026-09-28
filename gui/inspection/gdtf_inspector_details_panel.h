#pragma once

#include "inspection/gdtf_inspection.h"

#include <optional>

#include <wx/panel.h>

class wxChoice;
class wxNotebook;
class wxTextCtrl;

namespace gui::inspection {

// Presents immutable GDTF metadata, mode/channel structure, and wheel details.
class GdtfInspectorDetailsPanel final : public wxPanel {
public:
  explicit GdtfInspectorDetailsPanel(wxWindow *parent);

  void ClearResult();
  void SetAvailable(bool available);
  void SetResult(const perastage::inspection::GdtfInspectionResult &result);
  int SelectedPage() const;
  void SetSelectedPage(int page);

private:
  void BuildLayout();
  void SelectMode(int selection);

  wxNotebook *pages_ = nullptr;
  wxTextCtrl *overview_ = nullptr;
  wxChoice *modes_ = nullptr;
  wxTextCtrl *modeDetails_ = nullptr;
  wxTextCtrl *wheels_ = nullptr;
  std::optional<gdtf::GdtfModeChannelDocument> modeDocument_;
};

} // namespace gui::inspection
