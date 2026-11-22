#pragma once
#include "fixture_label_composition.h"
#include "fixture_label_preview_geometry.h"
#include <wx/dialog.h>
#include <wx/listbox.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <wx/spinctrl.h>
#include <wx/clrpicker.h>
#include <functional>

class FixtureLabelPreview;
struct FixtureLabelEditSession {
  wxString defaultsScope, fixturesScope;
  viewer2d::FixtureLabelOverrideMap draft, inherited;
  std::function<void(const viewer2d::FixtureLabelOverrideMap &)> commit;
  std::optional<std::vector<std::string>> fixtureSelection;
};
namespace gui { class DocumentFontChoice; }

class FixtureLabelEditor : public wxDialog {
public:
  FixtureLabelEditor(wxWindow *parent, ConfigManager &cfg, int initialView,
                     const FixtureLabelEditSession *session = nullptr);
  ~FixtureLabelEditor() override;
private:
  viewer2d::FixtureLabelOverrideMap EffectiveDraft() const;
  std::optional<FixtureLabelEditSession> session_;
  void LoadField();
  void StoreField(wxObject *source);
  void Apply();
  void Reset();
  void RefreshPreview();
  void OnPreviewDrag(label_preview::DragPhase, const viewer2d::LabelFieldStyle &);
  ConfigManager &cfg_;
  viewer2d::FixtureLabelOverrideMap draft_;
  std::vector<std::string> selection_;
  Fixture fixture_;
  wxChoice *scope_ = nullptr, *view_ = nullptr, *anchor_ = nullptr, *align_ = nullptr;
  gui::DocumentFontChoice *family_ = nullptr;
  wxListBox *fields_ = nullptr;
  wxCheckBox *visible_ = nullptr, *bold_ = nullptr, *italic_ = nullptr;
  wxSpinCtrlDouble *size_ = nullptr, *x_ = nullptr, *y_ = nullptr;
  wxColourPickerCtrl *color_ = nullptr;
  wxCheckBox *outline_ = nullptr;
  wxColourPickerCtrl *outlineColor_ = nullptr;
  wxSpinCtrlDouble *outlineWidth_ = nullptr;
  FixtureLabelPreview *preview_ = nullptr;
  bool loading_ = false, changed_ = false;
  std::optional<viewer2d::FixtureLabelOverrideMap> dragDraft_;
  bool changedBeforeDrag_ = false;
  std::string ScopeUuid() const;
};
