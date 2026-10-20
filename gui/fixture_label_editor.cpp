#include "fixture_label_editor.h"
#include "fixture_label_preview.h"
#include "viewer2dpanel.h"
#include "layoutviewerpanel.h"
#include "mainwindow.h"
#include <algorithm>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/button.h>

FixtureLabelEditor::FixtureLabelEditor(wxWindow *parent, ConfigManager &cfg, int initialView)
    : wxDialog(parent, wxID_ANY, _("Fixture Label Editor"), wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), cfg_(cfg),
      draft_(viewer2d::LoadFixtureLabelOverrides(cfg)), selection_(cfg.GetSelectedFixtures()) {
  if (!selection_.empty() && cfg.GetScene().fixtures.contains(selection_.front()))
    fixture_ = cfg.GetScene().fixtures.at(selection_.front());
  else if (!cfg.GetScene().fixtures.empty()) {
    auto it = std::min_element(cfg.GetScene().fixtures.begin(), cfg.GetScene().fixtures.end(),
      [](const auto &a, const auto &b) { return a.first < b.first; });
    fixture_ = it->second;
  } else { fixture_.uuid = "preview"; fixture_.instanceName = "Fixture"; fixture_.fixtureId = 1; fixture_.address = "1.001"; }
  auto *outer = new wxBoxSizer(wxVERTICAL);
  auto *header = new wxBoxSizer(wxHORIZONTAL);
  scope_ = new wxChoice(this, wxID_ANY);
  scope_->Append(_("Project defaults"));
  if (!selection_.empty()) scope_->Append(_("Selected fixtures"));
  scope_->SetSelection(selection_.empty() ? 0 : 1);
  view_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxArrayString{_("Top"), _("Front"), _("Side")});
  view_->SetSelection(std::clamp(initialView, 0, 2));
  header->Add(scope_, 1, wxRIGHT, 8); header->Add(view_, 0);
  outer->Add(header, 0, wxEXPAND | wxALL, 10);
  auto *body = new wxBoxSizer(wxHORIZONTAL);
  fields_ = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(130, -1),
                        wxArrayString{_("Fixture Name"), _("Fixture ID"), _("DMX Address")});
  fields_->SetSelection(0); body->Add(fields_, 0, wxEXPAND | wxRIGHT, 10);
  auto *properties = new wxFlexGridSizer(2, 6, 8); properties->AddGrowableCol(1);
  const auto row = [&](const wxString &label, wxWindow *control) {
    properties->Add(new wxStaticText(this, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
    properties->Add(control, 1, wxEXPAND);
  };
  visible_ = new wxCheckBox(this, wxID_ANY, _("Show field")); row(_("Visibility"), visible_);
  auto *family = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxArrayString{"PerastageSans (Noto Sans)"});
  family->SetSelection(0); row(_("Font family"), family);
  bold_ = new wxCheckBox(this, wxID_ANY, _("Bold")); row(_("Style"), bold_);
  const auto spin = [&](double min, double max, double step) {
    auto *s = new wxSpinCtrlDouble(this, wxID_ANY, "", wxDefaultPosition, wxSize(100,-1), wxSP_ARROW_KEYS);
    s->SetRange(min,max); s->SetIncrement(step); s->SetDigits(3); return s;
  };
  size_ = spin(0.004, 4, 0.01); row(_("Size (m)"), size_);
  color_ = new wxColourPickerCtrl(this, wxID_ANY); row(_("Color"), color_);
  anchor_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
    wxArrayString{_("Center"), _("Top"), _("Bottom"), _("Left"), _("Right")}); row(_("Anchor"), anchor_);
  align_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
    wxArrayString{_("Left"), _("Center"), _("Right")}); row(_("Alignment"), align_);
  x_ = spin(-100,100,0.01); row(_("X offset (m)"), x_);
  y_ = spin(-100,100,0.01); row(_("Y offset (m)"), y_);
  body->Add(properties, 0, wxRIGHT, 10);
  auto *previewBox = new wxBoxSizer(wxVERTICAL);
  auto *previewStatus = new wxStaticText(this, wxID_ANY, wxString(), wxDefaultPosition, wxSize(290,36));
  preview_ = new FixtureLabelPreview(this, cfg, *previewStatus);
  previewBox->Add(preview_, 1, wxEXPAND);
  previewBox->Add(previewStatus, 0, wxEXPAND | wxTOP, 6);
  body->Add(previewBox, 1, wxEXPAND);
  outer->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
  auto *note = new wxStaticText(this, wxID_ANY,
      _("Legacy rendering is preserved until edited; its preview is approximate. Sizes and offsets use scene metres.\nSupported text: English and Western European WinAnsi characters. No viewport-specific styles yet."));
  note->Wrap(700); outer->Add(note, 0, wxEXPAND | wxALL, 10);
  auto *buttons = new wxBoxSizer(wxHORIZONTAL);
  auto *reset = new wxButton(this, wxID_RESET, _("Reset to Default"));
  buttons->Add(reset, 0, wxRIGHT, 8); buttons->AddStretchSpacer();
  buttons->Add(CreateButtonSizer(wxAPPLY | wxOK | wxCANCEL));
  outer->Add(buttons, 0, wxEXPAND | wxALL, 10); SetSizerAndFit(outer);
  SetMinSize(wxSize(760,460)); SetSize(wxSize(850,510)); CentreOnParent();
  for (auto *choice : {scope_, view_}) choice->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) { LoadField(); });
  fields_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent &) { LoadField(); });
  for (auto *choice : {anchor_,align_}) choice->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) { StoreField(); });
  for (auto *check : {visible_,bold_}) check->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &) { StoreField(); });
  for (auto *s : {size_,x_,y_}) {
    s->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent &) { StoreField(); });
    s->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { StoreField(); });
  }
  color_->Bind(wxEVT_COLOURPICKER_CHANGED, [this](wxColourPickerEvent &) { StoreField(); });
  Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Apply(); }, wxID_APPLY);
  Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Apply(); EndModal(wxID_OK); }, wxID_OK);
  reset->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Reset(); });
  LoadField();
}
std::string FixtureLabelEditor::ScopeUuid() const {
  return scope_->GetSelection() == 0 ? viewer2d::kLabelDefaultsKey : selection_.front();
}
void FixtureLabelEditor::LoadField() {
  loading_ = true;
  const auto s = viewer2d::ResolveFieldStyle(cfg_, draft_, ScopeUuid(), view_->GetSelection(), fields_->GetSelection());
  visible_->SetValue(s.visible); bold_->SetValue(s.bold); size_->SetValue(s.size);
  x_->SetValue(s.x); y_->SetValue(s.y); anchor_->SetSelection(s.anchor); align_->SetSelection(s.alignment);
  color_->SetColour(wxColour(s.color[0]*255,s.color[1]*255,s.color[2]*255));
  loading_ = false; RefreshPreview();
}
void FixtureLabelEditor::StoreField() {
  if (loading_) return;
  double size, x, y;
  if (!size_->GetTextValue().ToDouble(&size) || !x_->GetTextValue().ToDouble(&x) ||
      !y_->GetTextValue().ToDouble(&y) || !std::isfinite(size) || !std::isfinite(x) || !std::isfinite(y) || size < .004 || size > 4 ||
      std::abs(x)>100 || std::abs(y)>100) return;
  viewer2d::LabelFieldStyle s;
  s.visible = visible_->GetValue(); s.bold = bold_->GetValue(); s.size = size;
  s.x = x; s.y = y; s.anchor = anchor_->GetSelection(); s.alignment = align_->GetSelection();
  const auto c = color_->GetColour(); s.color = {c.Red()/255.f,c.Green()/255.f,c.Blue()/255.f};
  const auto ids = scope_->GetSelection() == 0 ? std::vector<std::string>{viewer2d::kLabelDefaultsKey} : selection_;
  for (const auto &id : ids) draft_[id].fieldStyles[view_->GetSelection()][fields_->GetSelection()] = s;
  changed_ = true; RefreshPreview();
}
void FixtureLabelEditor::RefreshPreview() {
  auto preview = draft_;
  if (scope_->GetSelection() == 0) preview.erase(fixture_.uuid);
  preview_->SetContent(fixture_, view_->GetSelection(), preview);
}
void FixtureLabelEditor::Apply() {
  if (!changed_) return;
  if (viewer2d::CommitFixtureLabelStyles(cfg_, draft_)) {
    if (auto *panel = Viewer2DPanel::Instance()) panel->UpdateScene(false);
    if (auto *window = MainWindow::Instance()) { window->RefreshAfterFixtureSymbolUpdate(); }
  }
  changed_ = false;
}
void FixtureLabelEditor::Reset() {
  const auto ids = scope_->GetSelection() == 0 ? std::vector<std::string>{viewer2d::kLabelDefaultsKey} : selection_;
  for (const auto &id : ids) draft_.erase(id);
  changed_ = true; LoadField();
}
