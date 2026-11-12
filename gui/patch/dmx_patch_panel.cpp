/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "patch/dmx_patch_panel.h"
#include "patch/dmx_patch_canvas.h"
#include "configmanager.h"
#include "fixtures/fixture_gdtf_resolution.h"
#include "gdtf_archive_reader.h"
#include <wx/combobox.h>
#include <wx/bmpbuttn.h>
#include <wx/checkbox.h>
#include <wx/artprov.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <algorithm>

namespace {
wxString Text(const std::string &s) { return wxString::FromUTF8(s); }
}

DmxPatchPanel::DmxPatchPanel(wxWindow *parent, ConfigManager &config)
    : wxPanel(parent), config_(config) {
  SetName(_("DMX Patch"));
  auto *outer = new wxBoxSizer(wxVERTICAL);
  auto *bar = new wxBoxSizer(wxHORIZONTAL);
  bar->Add(new wxStaticText(this, wxID_ANY, _("Universe:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
  universeControl_ = new wxComboBox(this, wxID_ANY, "1", wxDefaultPosition, FromDIP(wxSize(105, -1)), 0, nullptr, wxTE_PROCESS_ENTER);
  universeControl_->SetName(_("DMX universe"));
  universeControl_->SetToolTip(_("Choose a patched universe or enter any positive universe number."));
  bar->Add(universeControl_, 0, wxRIGHT, FromDIP(6));
  compatibility_ = new wxCheckBox(this, wxID_ANY, _("Declared offsets"));
  compatibility_->SetToolTip(_("Explicit provisional preview for unresolved GDTF channels; does not change the patch."));
  compatibility_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &) { RefreshSnapshot(); });
  bar->Add(compatibility_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
  status_ = new wxStaticText(this, wxID_ANY, _("DMX Patch"), wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
  status_->SetMinSize(wxSize(1, status_->GetCharHeight() + FromDIP(4)));
  bar->Add(status_, 1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(4));
  warnings_ = new wxBitmapButton(this, wxID_ANY,
      wxArtProvider::GetBitmap(wxART_WARNING, wxART_BUTTON, FromDIP(wxSize(16, 16))));
  warnings_->SetName(_("Patch diagnostics"));
  warnings_->SetToolTip(_("Show patch diagnostics"));
  warnings_->Disable();
  warnings_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { ShowDiagnostics(); });
  bar->Add(warnings_, 0, wxALIGN_CENTER_VERTICAL);
  outer->Add(bar, 0, wxEXPAND | wxALL, FromDIP(4));
  canvas_ = new DmxPatchCanvas(this);
  outer->Add(canvas_, 1, wxEXPAND);
  SetSizer(outer);
  universeControl_->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) { DisplayUniverse(); });
  universeControl_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) { DisplayUniverse(); });
  universeControl_->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) { DisplayUniverse(); e.Skip(); });
  subscription_ = config_.SubscribeChanges([this](ConfigManager::Change change) {
    if (change == ConfigManager::Change::Scene) { stale_ = true; QueueRefresh(); }
  });
  Bind(wxEVT_SHOW, [this](wxShowEvent &e) { if (e.IsShown() && stale_) QueueRefresh(); e.Skip(); });
  QueueRefresh();
}
DmxPatchPanel::~DmxPatchPanel() { config_.UnsubscribeChanges(subscription_); }
void DmxPatchPanel::QueueRefresh() {
  if (queued_) return;
  queued_ = true;
  CallAfter([this] { queued_ = false; if (IsShownOnScreen()) RefreshSnapshot(); });
}
void DmxPatchPanel::DisplayUniverse() {
  const auto address = dmx_patch::ReadAddress(universeControl_->GetValue().ToStdString() + ".1");
  if (!address) {
    universeControl_->SetToolTip(_("Enter a positive universe number."));
    return;
  }
  universe_ = address->universe;
  universeControl_->SetToolTip(_("Choose a patched universe or enter any positive universe number."));
  RefreshSnapshot();
}
std::shared_ptr<const gdtf::GdtfModeChannelDocument> DmxPatchPanel::Resolve(const Fixture &fixture) {
  auto path = paths_.find(fixture.gdtfSpec);
  if (path == paths_.end()) {
    gui::fixtures::FixtureGdtfResolution resolution;
    std::string error;
    gui::fixtures::ResolveFixtureGdtfDeterministic(fixture, static_cast<const ConfigManager &>(config_).GetScene(), resolution, error, "DMX Patch");
    path = paths_.emplace(fixture.gdtfSpec, resolution.selectedPath).first;
  }
  if (path->second.empty()) return {};
  std::error_code error;
  const auto modified = std::filesystem::last_write_time(path->second, error);
  if (error) return {};
  const auto size = std::filesystem::file_size(path->second, error);
  if (error) return {};
  auto &cached = cache_[path->second];
  if (!cached.document || cached.modified != modified || cached.size != size) {
    const auto archive = gdtf::ReadGdtfArchive(path->second);
    auto document = std::make_shared<gdtf::GdtfModeChannelDocument>();
    if (archive.Success()) *document = gdtf::ReadGdtfModeChannelDocument(archive.descriptionXml);
    cached = {modified, size, std::move(document)};
  }
  return cached.document;
}
void DmxPatchPanel::RefreshSnapshot() {
  const auto &scene = static_cast<const ConfigManager &>(config_).GetScene();
  const bool newProject = !projectKnown_ || projectBase_ != scene.basePath;
  projectKnown_ = true;
  if (newProject) { projectBase_ = scene.basePath; universe_ = 1; cache_.clear(); }
  paths_.clear();
  auto resolver = [this](const Fixture &f) { return Resolve(f); };
  const auto preview = compatibility_->GetValue() ? dmx_patch::Preview::DeclaredOffsets : dmx_patch::Preview::ResolvedOnly;
  auto snapshot = std::make_shared<dmx_patch::Snapshot>(dmx_patch::BuildSnapshot(scene, universe_, resolver, preview));
  if (newProject && !snapshot->universes.empty() && !snapshot->universes.contains(universe_)) {
    universe_ = *snapshot->universes.begin();
    *snapshot = dmx_patch::BuildSnapshot(scene, universe_, resolver, preview);
  }
  for (auto it = cache_.begin(); it != cache_.end();) {
    const bool used = std::any_of(paths_.begin(), paths_.end(), [&](const auto &entry) { return entry.second == it->first; });
    if (!used) it = cache_.erase(it); else ++it;
  }
  universeControl_->Clear();
  for (auto universe : snapshot->universes) universeControl_->Append(Text(std::to_string(universe)));
  universeControl_->ChangeValue(Text(std::to_string(universe_)));
  status_->SetLabel(wxString::Format(_("Occupied: %d/512 | Conflicts: %d"), snapshot->occupied, snapshot->conflicts));
  if (compatibility_->GetValue())
    status_->SetLabel(wxString::Format(_("Occupied: %d/512 | Provisional: %d | Conflicts: %d"),
        snapshot->occupied, snapshot->provisional, snapshot->conflicts));
  status_->SetToolTip(status_->GetLabel());
  warnings_->Enable(!snapshot->diagnostics.empty() || snapshot->conflicts > 0);
  warnings_->SetToolTip(wxString::Format(_("Show patch diagnostics (%d)"),
      static_cast<int>(snapshot->diagnostics.size()) + snapshot->conflicts));
  snapshot_ = snapshot;
  canvas_->SetSnapshot(snapshot);
  stale_ = false;
  Layout();
}
