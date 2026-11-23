#pragma once

#include <optional>
#include <wx/combo.h>

class wxPanel;
class wxStaticBitmap;
class wxTextCtrl;

namespace gui {
class DocumentFontChoice;
class FontFamilyList;

class DocumentFontPopup : public wxComboPopup {
public:
  explicit DocumentFontPopup(DocumentFontChoice &choice) : choice_(choice) {}
  bool Create(wxWindow *parent) override;
  wxWindow *GetControl() override;
  wxString GetStringValue() const override;
  bool LazyCreate() override { return true; }
  void OnPopup() override;
  void OnDismiss() override;
  void OnComboKeyEvent(wxKeyEvent &event) override;
  wxSize GetAdjustedSize(int minWidth, int prefHeight, int maxHeight) override;

private:
  void Filter();
  void ClearPreview();
  void Highlight(int row);
  void Move(int direction);
  void Confirm(int row);
  void OnKey(wxKeyEvent &event);

  DocumentFontChoice &choice_;
  wxPanel *panel_ = nullptr;
  wxTextCtrl *search_ = nullptr;
  FontFamilyList *list_ = nullptr;
  wxStaticBitmap *sample_ = nullptr;
  std::optional<size_t> highlighted_;
  bool active_ = false, previewActive_ = false;
};
}
