#include "preferences/preferences_page.h"

#include <algorithm>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/textwrapper.h>

namespace {
class LabelWrapper : public wxTextWrapper {
public:
  wxString text;
private:
  void OnOutputLine(const wxString &line) override { text += line; }
  void OnNewLine() override { text += "\n"; }
};
} // namespace

PreferencesPage::PreferencesPage(wxWindow *parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxVSCROLL | wxTAB_TRAVERSAL) {
  SetScrollRate(0, FromDIP(10));
  Bind(wxEVT_SIZE, [this](wxSizeEvent &event) {
    ReflowLabels();
    event.Skip();
  });
}

void PreferencesPage::CollectWrappingLabels(wxWindow *window) {
  for (wxWindow *child : window->GetChildren()) {
    if (auto *choice = dynamic_cast<wxChoice *>(child))
      choice->SetMinSize(wxSize(FromDIP(150), -1));
    const bool wraps = dynamic_cast<wxStaticText *>(child) ||
                       dynamic_cast<wxCheckBox *>(child) ||
                       dynamic_cast<wxRadioButton *>(child);
    if (wraps && child->GetLabel().size() > 55)
      wrappingLabels.emplace_back(static_cast<wxControl *>(child), child->GetLabel());
    CollectWrappingLabels(child);
  }
}

void PreferencesPage::PrepareLayout() {
  CollectWrappingLabels(this);
  ReflowLabels();
}

void PreferencesPage::InvalidateLayoutSizes(wxWindow *window) {
  for (wxWindow *child : window->GetChildren())
    InvalidateLayoutSizes(child);
  window->InvalidateBestSize();
  // wxStaticBoxSizer consults GetBestSize(), rather than GetMinSize(), for
  // the title width. GTK can include old child allocations in that width;
  // content dimensions must instead come from the static-box sizer itself.
  if (auto *box = dynamic_cast<wxStaticBox *>(window))
    box->CacheBestSize(wxSize(box->GetTextExtent(box->GetLabel()).x + FromDIP(24),
                              FromDIP(24)));
}

void PreferencesPage::ReflowLabels() {
  if (!GetSizer())
    return;
  const int width = std::max(FromDIP(160), GetClientSize().x - FromDIP(64));
  for (auto &[label, originalText] : wrappingLabels) {
    LabelWrapper wrapper;
    wrapper.Wrap(label, originalText, width);
    if (label->GetLabel() != wrapper.text)
      label->SetLabel(wrapper.text);
    label->SetMinSize(wxDefaultSize);
  }
  InvalidateLayoutSizes(this);
  Layout();
  FitInside();
}
