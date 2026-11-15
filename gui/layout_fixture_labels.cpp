/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "layout_fixture_labels.h"
#include "fixture_label_editor.h"
#include "fixture_label_presentation.h"
#include "guiconfigservices.h"
#include "LayoutManager.h"
#include "layoutviewerpanel.h"
#include "viewer2dpanel.h"
#include "viewer2dstate.h"
#include "uuidutils.h"
#include "projectutils.h"
#include <wx/button.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textdlg.h>

namespace {
using namespace layouts;
auto &Config() { return GetDefaultGuiConfigServices().LegacyConfigManager(); }
viewer2d::FixtureLabelOverrideMap LocalDraft(const FixtureLabelPresentation &p) {
  viewer2d::FixtureLabelOverrideMap draft;
  draft[viewer2d::kLabelDefaultsKey].fieldStyles = p.fields;
  for (const auto &[id, fields] : p.fixtures) draft[id].fieldStyles = fields;
  return draft;
}
void StoreLocal(FixtureLabelPresentation &p, const viewer2d::FixtureLabelOverrideMap &draft) {
  p.fields = {}; p.fixtures.clear();
  for (const auto &[id, entry] : draft) {
    if (id == viewer2d::kLabelDefaultsKey) p.fields = entry.fieldStyles;
    else if (entry.HasAnyValue()) p.fixtures[id] = entry.fieldStyles;
  }
}
void EditLocal(wxWindow *parent, FixtureLabelPresentation &p, const FixtureLabelProfiles &profiles, int view, const Layout2DViewRenderOptions &options) {
  auto base = p; base.fields = {}; base.fixtures.clear();
  const auto inherited = viewer2d::OverlayLabelPresentation(
      viewer2d::SnapshotLayoutLabelEditorInheritance(Config(), options), base, profiles);
  FixtureLabelEditSession session{_("Current Layout viewport"), _("Selected fixtures in current Layout viewport"),
    LocalDraft(p), inherited,
    [&](const auto &draft) { StoreLocal(p, draft); }};
  FixtureLabelEditor editor(parent, Config(), view, &session);
  editor.ShowModal();
}
class ViewLabelDialog : public wxDialog {
public:
  Layout2DViewDefinition view;
  FixtureLabelProfiles profiles;
  ViewLabelDialog(wxWindow *parent, const Layout2DViewDefinition &source)
      : wxDialog(parent, wxID_ANY, _("Layout viewport fixture labels")), view(source),
        profiles(LayoutManager::Get().GetLayouts().labelProfiles) {
    auto *box = new wxBoxSizer(wxVERTICAL);
    choice = new wxChoice(this, wxID_ANY);
    box->Add(choice, 0, wxEXPAND | wxALL, 10);
    localStatus = new wxStaticText(this, wxID_ANY, wxString());
    box->Add(localStatus, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    const auto button = [&](const wxString &title, auto action) {
      auto *b = new wxButton(this, wxID_ANY, title);
      box->Add(b, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
      b->Bind(wxEVT_BUTTON, [action](wxCommandEvent &) { action(); });
    };
    button(_("Customize current viewport..."), [this] { SyncAssignment(); EditLocal(this, view.labelPresentation, profiles, view.camera.view, view.renderOptions); Populate(); });
    button(_("Create style from current settings..."), [this] { Create(false); });
    button(_("Edit named style..."), [this] { EditProfile(); });
    button(_("Duplicate named style..."), [this] { Create(true); });
    button(_("Rename named style..."), [this] {
      SyncAssignment(); auto it=profiles.find(view.labelPresentation.profileId); if (it==profiles.end()) return;
      wxTextEntryDialog name(this, _("Style name"), _("Rename named style"), wxString::FromUTF8(it->second.name));
      if (name.ShowModal()==wxID_OK && !name.GetValue().Strip().empty()) { it->second.name=name.GetValue().Strip().ToStdString(wxConvUTF8); Populate(); }
    });
    button(_("Delete unused named style"), [this] {
      SyncAssignment(); const auto id=view.labelPresentation.profileId; if (id.empty()) return;
      for (const auto &page : LayoutManager::Get().GetLayouts().Items()) for (const auto &v : page.view2dViews)
        if (v.labelPresentation.profileId==id) {
          wxMessageBox(_("This style is referenced by a Layout viewport. Restore inherited settings in every referencing viewport before deleting it."), _("Fixture Label Editor"), wxOK | wxICON_INFORMATION, this); return;
        }
      profiles.erase(id); view.labelPresentation.profileId.clear(); Populate();
    });
    button(_("Restore inherited settings"), [this] { view.labelPresentation = {}; Populate(); });
    box->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
    SetSizerAndFit(box); Populate(); CentreOnParent();
  }
  void SyncAssignment() {
    const auto n=choice->GetSelection();
    view.labelPresentation.profileId = n>0 ? ids.at(n-1) : std::string{};
  }
private:
  wxChoice *choice;
  wxStaticText *localStatus;
  std::vector<std::string> ids;
  void Populate() {
    choice->Clear(); choice->Append(_("Inherit project presentation")); ids.clear();
    int selected=0;
    for (const auto &[id, profile] : profiles) {
      ids.push_back(id); choice->Append(wxString::FromUTF8(profile.name));
      if (id==view.labelPresentation.profileId) selected=ids.size();
    }
    choice->SetSelection(selected);
    bool local = !view.labelPresentation.fixtures.empty();
    for (const auto &orientation : view.labelPresentation.fields) for (const auto &field : orientation) local |= field.has_value();
    localStatus->SetLabel(local ? _("Viewport-local adjustments are active.") : wxString());
  }
  void Create(bool duplicate) {
    SyncAssignment();
    auto it=profiles.find(view.labelPresentation.profileId);
    if (duplicate && it==profiles.end()) return;
    wxTextEntryDialog name(this, _("Style name"), _("Create named style"));
    if (name.ShowModal()!=wxID_OK || name.GetValue().Strip().empty()) return;
    FixtureLabelStyleProfile profile{GenerateUuid(), name.GetValue().Strip().ToStdString(wxConvUTF8), {}};
    if (duplicate) profile.fields=it->second.fields;
    else {
      const auto all=viewer2d::OverlayLabelPresentation(
          viewer2d::SnapshotLayoutLabelEditorInheritance(Config(), view.renderOptions), view.labelPresentation, profiles);
      const typography::FontPair fonts(ProjectUtils::GetResourceRoot());
      for (int v=0; v<3; ++v) {
        const auto styles=viewer2d::ResolveEditableFieldStyles(Config(), all, viewer2d::kLabelDefaultsKey, v, fonts);
        for (int f=0; f<3; ++f) profile.fields[v][f]=styles[f];
      }
    }
    if (!duplicate) view.labelPresentation.fields={};
    view.labelPresentation.profileId=profile.id;
    profiles.emplace(profile.id, std::move(profile)); Populate();
  }
  void EditProfile() {
    SyncAssignment(); auto it=profiles.find(view.labelPresentation.profileId); if (it==profiles.end()) return;
    FixtureLabelEditSession session{_("Named style profile"), {}, {}, viewer2d::SnapshotLayoutLabelEditorInheritance(Config(), view.renderOptions),
      [&](const auto &draft) { const auto d=draft.find(viewer2d::kLabelDefaultsKey); it->second.fields = d==draft.end() ? viewer2d::LabelFieldStyles{} : d->second.fieldStyles; }};
    session.draft[viewer2d::kLabelDefaultsKey].fieldStyles=it->second.fields;
    FixtureLabelEditor editor(this, Config(), view.camera.view, &session);
    editor.ShowModal();
  }
};
} // namespace

void AppendLayoutFixtureLabelMenu(wxMenu &menu, LayoutViewerPanel &panel, const std::string &pageName, int viewId) {
  auto *item=menu.Append(wxID_ANY, _("Fixture label settings..."));
  menu.Bind(wxEVT_MENU, [&panel, pageName, viewId](wxCommandEvent &) {
    auto &manager=layouts::LayoutManager::Get();
    for (const auto &page : manager.GetLayouts().Items()) if (page.name==pageName)
      for (const auto &view : page.view2dViews) if (view.id==viewId) {
        ViewLabelDialog dialog(&panel, view);
        if (dialog.ShowModal()!=wxID_OK) return;
        dialog.SyncAssignment();
        if (!manager.CommitLabelPresentation(pageName, dialog.view, dialog.profiles)) {
          wxMessageBox(_("Could not update fixture label settings."), _("Fixture Label Editor"), wxOK | wxICON_ERROR, &panel); return;
        }
        for (const auto &updated : manager.GetLayouts().Items()) if (updated.name==pageName) {
          panel.SetLayoutDefinition(updated); panel.Refresh(false); break;
        }
        return;
      }
  }, item->GetId());
}

void EditPanelFixtureLabels(wxWindow *parent, Viewer2DPanel *panel, int view) {
  if (panel && panel->GetLayoutEditOverlaySize()) {
    auto draft=panel->labelPresentation;
    EditLocal(parent, draft, layouts::LayoutManager::Get().GetLayouts().labelProfiles, view,
        viewer2d::CaptureState(panel, Config()).renderOptions);
    panel->labelPresentation=std::move(draft);
    panel->SetFixtureLabelPresentationContext(viewer2d::BuildLabelPresentationContext(Config(), panel->labelPresentation,
        layouts::LayoutManager::Get().GetLayouts().labelProfiles));
    panel->Refresh(false);
  } else {
    FixtureLabelEditor editor(parent, Config(), view); editor.ShowModal();
  }
}
