#include "fixture_label_editor.h"
#include "fixture_label_preview.h"
#include "document_font_choice.h"
#include "viewer2dpanel.h"
#include "layoutviewerpanel.h"
#include "mainwindow.h"
#include <algorithm>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/panel.h>

FixtureLabelEditor::FixtureLabelEditor(wxWindow *parent, ConfigManager &cfg, int initialView,
                                       const FixtureLabelEditSession *session)
    : wxDialog(parent, wxID_ANY, _("Fixture Label Editor"), wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), cfg_(cfg),
      draft_(session ? session->draft : viewer2d::LoadFixtureLabelOverrides(cfg)), selection_(session && session->fixtureSelection ? *session->fixtureSelection : cfg.GetSelectedFixtures()) {
  if (session) session_ = *session;
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
  scope_->Append(session ? session->defaultsScope : _("Project defaults"));
  if (session && session->fixturesScope.empty()) selection_.clear();
  if (!selection_.empty()) scope_->Append(session ? session->fixturesScope : _("Selected project fixtures"));
  scope_->SetName("fixtureLabelScope");
  scope_->SetSelection(!selection_.empty() && (!session || session->fixtureSelection) ? 1 : 0);
  view_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxArrayString{_("Top"), _("Front"), _("Side")});
  view_->SetSelection(std::clamp(initialView, 0, 2));
  view_->SetName("fixtureLabelView");
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
  family_ = new gui::DocumentFontChoice(this, {});
  family_->SetName("fixtureLabelFontFamily");
  row(_("Font family"), family_);
  auto *styles = new wxPanel(this);
  auto *styleRow = new wxBoxSizer(wxHORIZONTAL);
  bold_ = new wxCheckBox(styles, wxID_ANY, _("Bold"));
  italic_ = new wxCheckBox(styles, wxID_ANY, _("Italic"));
  styleRow->Add(bold_, 0, wxRIGHT, 8);
  styleRow->Add(italic_);
  styles->SetSizerAndFit(styleRow);
  row(_("Style"), styles);
  const auto spin = [&](double min, double max, double step) {
    auto *s = new wxSpinCtrlDouble(this, wxID_ANY, "", wxDefaultPosition, wxSize(100,-1), wxSP_ARROW_KEYS);
    s->SetRange(min,max); s->SetIncrement(step); s->SetDigits(3); return s;
  };
  size_ = spin(0.004, 4, 0.01); row(_("Size (m)"), size_);
  color_ = new wxColourPickerCtrl(this, wxID_ANY); row(_("Color"), color_);
  outline_ = new wxCheckBox(this, wxID_ANY, _("Enabled")); row(_("Outline"), outline_);
  outlineColor_ = new wxColourPickerCtrl(this, wxID_ANY); row(_("Outline color"), outlineColor_);
  outlineWidth_ = spin(0,1,0.001); row(_("Outline thickness (m)"), outlineWidth_);
  outlineWidth_->SetName("fixtureLabelOutlineWidth");
  outlineColor_->SetName("fixtureLabelOutlineColor");
  anchor_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
    wxArrayString{_("Center"), _("Top"), _("Bottom"), _("Left"), _("Right")}); row(_("Anchor"), anchor_);
  align_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
    wxArrayString{_("Left"), _("Center"), _("Right")}); row(_("Alignment"), align_);
  x_ = spin(-100,100,0.01); row(_("X offset (m)"), x_);
  y_ = spin(-100,100,0.01); row(_("Y offset (m)"), y_);
  x_->SetName("fixtureLabelOffsetX"); y_->SetName("fixtureLabelOffsetY");
  body->Add(properties, 0, wxRIGHT, 10);
  auto *previewBox = new wxBoxSizer(wxVERTICAL);
  auto *backgroundRow = new wxBoxSizer(wxHORIZONTAL);
  backgroundRow->Add(new wxStaticText(this,wxID_ANY,_("Preview background")),0,wxALIGN_CENTER_VERTICAL | wxRIGHT,6);
  auto *background = new wxChoice(this,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxArrayString{_("White"),_("Dark")});
  background->SetSelection(0); backgroundRow->Add(background);
  background->SetName("fixtureLabelPreviewBackground");
  previewBox->Add(backgroundRow,0,wxEXPAND | wxBOTTOM,6);
  auto *previewStatus = new wxStaticText(this, wxID_ANY, wxString(), wxDefaultPosition, wxSize(290,48));
  preview_ = new FixtureLabelPreview(this, cfg, *previewStatus);
  previewBox->Add(preview_, 1, wxEXPAND);
  previewBox->Add(previewStatus, 0, wxEXPAND | wxTOP, 6);
  previewBox->Add(new wxStaticText(this,wxID_ANY,_("Drag the selected field in the preview.")),0,wxTOP,4);
  body->Add(previewBox, 1, wxEXPAND);
  outer->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
  auto *note = new wxStaticText(this, wxID_ANY,
      _("Sizes and offsets use scene metres. PDF export reports installed fonts that require substitution."));
  note->Wrap(700); outer->Add(note, 0, wxEXPAND | wxALL, 10);
  auto *buttons = new wxBoxSizer(wxHORIZONTAL);
  auto *reset = new wxButton(this, wxID_RESET, _("Reset to Default"));
  buttons->Add(reset, 0, wxRIGHT, 8);
  auto *refreshFonts = new wxButton(this, wxID_ANY, _("Refresh fonts"));
  buttons->Add(refreshFonts, 0);
  refreshFonts->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
    const auto requested = family_->GetReference();
    gui::DocumentFontChoice::RefreshCatalog();
    family_->RefreshFamilies(requested);
    LoadField();
  });
  buttons->AddStretchSpacer();
  buttons->Add(CreateButtonSizer(wxAPPLY | wxOK | wxCANCEL));
  outer->Add(buttons, 0, wxEXPAND | wxALL, 10); SetSizerAndFit(outer);
  SetMinSize(wxSize(850,600)); SetSize(wxSize(930,650)); CentreOnParent();
  for (auto *choice : {scope_, view_}) choice->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) { preview_->CancelDrag(); preview_->ResetFit(); LoadField(); });
  fields_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent &) { preview_->CancelDrag(); LoadField(); });
  background->Bind(wxEVT_CHOICE,[this,background](wxCommandEvent &){ preview_->SetDarkBackground(background->GetSelection()==1); });
  for (auto *choice : {anchor_,align_,static_cast<wxChoice *>(family_)}) choice->Bind(wxEVT_CHOICE, [this](wxCommandEvent &event) { StoreField(event.GetEventObject()); event.Skip(); });
  for (auto *check : {visible_,bold_,italic_,outline_}) check->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &event) { StoreField(event.GetEventObject()); });
  for (auto *s : {size_,x_,y_,outlineWidth_}) {
    s->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent &event) { StoreField(event.GetEventObject()); });
    s->Bind(wxEVT_TEXT, [this](wxCommandEvent &event) { StoreField(event.GetEventObject()); });
  }
  color_->Bind(wxEVT_COLOURPICKER_CHANGED, [this](wxColourPickerEvent &event) { StoreField(event.GetEventObject()); });
  outlineColor_->Bind(wxEVT_COLOURPICKER_CHANGED, [this](wxColourPickerEvent &event) { StoreField(event.GetEventObject()); });
  preview_->SetDragCallback([this](auto phase,const auto &style){ OnPreviewDrag(phase,style); });
  Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Apply(); }, wxID_APPLY);
  Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Apply(); if (session_) session_->commit(draft_); if(IsModal())EndModal(wxID_OK);else Hide(); }, wxID_OK);
  Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { preview_->CancelDrag(); if(IsModal())EndModal(wxID_CANCEL);else Hide(); }, wxID_CANCEL);
  Bind(wxEVT_CLOSE_WINDOW,[this](wxCloseEvent &e){preview_->CancelDrag();e.Skip();});
  reset->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Reset(); });
  LoadField();
}
FixtureLabelEditor::~FixtureLabelEditor() {
  preview_->CancelDrag();
  preview_->SetDragCallback({});
}
std::string FixtureLabelEditor::ScopeUuid() const {
  return scope_->GetSelection() == 0 ? viewer2d::kLabelDefaultsKey : selection_.front();
}
void FixtureLabelEditor::LoadField() {
  loading_ = true;
  const auto s = preview_->ResolveEditingStyle(EffectiveDraft(), ScopeUuid(), view_->GetSelection(), fields_->GetSelection());
  family_->SelectFamily(s.fontFamily);
  visible_->SetValue(s.visible); bold_->SetValue(s.bold); italic_->SetValue(s.italic); size_->SetValue(s.size);
  x_->SetValue(s.x); y_->SetValue(s.y); anchor_->SetSelection(s.anchor); align_->SetSelection(s.alignment);
  color_->SetColour(wxColour(s.color[0]*255,s.color[1]*255,s.color[2]*255));
  outline_->SetValue(s.outlineEnabled); outlineWidth_->SetValue(s.outlineWidth);
  outlineColor_->SetColour(wxColour(s.outlineColor[0]*255,s.outlineColor[1]*255,s.outlineColor[2]*255));
  outlineColor_->Enable(s.outlineEnabled); outlineWidth_->Enable(s.outlineEnabled);
  loading_ = false; RefreshPreview();
}
void FixtureLabelEditor::StoreField(wxObject *source) {
  if (loading_) return;
  double size, x, y, width;
  if (!size_->GetTextValue().ToDouble(&size) || !x_->GetTextValue().ToDouble(&x) ||
      !y_->GetTextValue().ToDouble(&y) || !std::isfinite(size) || !std::isfinite(x) || !std::isfinite(y) || size < .004 || size > 4 ||
      std::abs(x)>100 || std::abs(y)>100 || !outlineWidth_->GetTextValue().ToDouble(&width) ||
      !std::isfinite(width) || width<0 || width>1) return;
  const auto ids = scope_->GetSelection() == 0 ? std::vector<std::string>{viewer2d::kLabelDefaultsKey} : selection_;
  for (const auto &id : ids) {
    auto s = preview_->ResolveEditingStyle(EffectiveDraft(), id, view_->GetSelection(), fields_->GetSelection());
    if (source == visible_) s.visible = visible_->GetValue();
    if (source == bold_) s.bold = bold_->GetValue();
    if (source == italic_) s.italic = italic_->GetValue();
    if (source == family_ && family_->GetSelection() != wxNOT_FOUND)
      s.fontFamily = family_->GetReference();
    if (source == size_) s.size = size;
    if (source == x_) s.x = x;
    if (source == y_) s.y = y;
    if (source == anchor_) s.anchor = anchor_->GetSelection();
    if (source == align_) s.alignment = align_->GetSelection();
    if (source == color_) {
      const auto c = color_->GetColour(); s.color = {c.Red()/255.f,c.Green()/255.f,c.Blue()/255.f};
    }
    if (source == outline_) s.outlineEnabled = outline_->GetValue();
    if (source == outlineWidth_) s.outlineWidth = width;
    if (source == outlineColor_) {
      const auto ink = outlineColor_->GetColour();
      s.outlineColor = {ink.Red()/255.f,ink.Green()/255.f,ink.Blue()/255.f};
    }
    draft_[id].fieldStyles[view_->GetSelection()][fields_->GetSelection()] = s;
  }
  outlineColor_->Enable(outline_->GetValue()); outlineWidth_->Enable(outline_->GetValue());
  changed_ = true; RefreshPreview();
}
void FixtureLabelEditor::RefreshPreview() {
  viewer2d::FixtureLabelOverrideMap preview;
  const auto effective = EffectiveDraft();
  for (const auto &key : {std::string(viewer2d::kLabelDefaultsKey),fixture_.uuid}) {
    if (key == fixture_.uuid && scope_->GetSelection() == 0 && !session_) continue;
    if (const auto it = effective.find(key); it != effective.end()) preview.emplace(*it);
  }
  preview_->SetActiveField(fields_->GetSelection());
  preview_->SetContent(fixture_, view_->GetSelection(), preview);
}
void FixtureLabelEditor::Apply() {
  preview_->CancelDrag();
  if (!changed_) return;
  if (session_) return;
  if (viewer2d::CommitFixtureLabelStyles(cfg_, draft_)) {
    if (auto *window = MainWindow::Instance()) window->RefreshAfterFixtureLabelUpdate();
  }
  changed_ = false;
}
void FixtureLabelEditor::Reset() {
  preview_->CancelDrag(); preview_->ResetFit();
  const auto ids = scope_->GetSelection() == 0 ? std::vector<std::string>{viewer2d::kLabelDefaultsKey} : selection_;
  for (const auto &id : ids) draft_.erase(id);
  changed_ = true; LoadField();
}
