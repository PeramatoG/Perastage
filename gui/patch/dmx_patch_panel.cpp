/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "patch/dmx_patch_panel.h"
#include "patch/dmx_patch_canvas.h"
#include "configmanager.h"
#include "fixtures/fixture_gdtf_resolution.h"
#include "gdtf_archive_reader.h"
#include <wx/combobox.h>
#include <wx/choice.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/wrapsizer.h>
#include <wx/stattext.h>
#include <algorithm>

namespace {
wxString Text(const std::string &s) { return wxString::FromUTF8(s); }
wxString IssueText(dmx_patch::Issue issue) {
  using dmx_patch::Issue;
  switch (issue) {
  case Issue::Unpatched: return _("Missing patch assignment");
  case Issue::InvalidAddress: return _("Invalid or unsupported DMX address");
  case Issue::InvalidBreak: return _("Invalid or ambiguous DMX break");
  case Issue::UnresolvedDefinition: return _("GDTF definition or DMX mode unavailable");
  case Issue::UnresolvedChannel: return _("Physical channel or geometry reference unresolved");
  case Issue::Overflow: return _("Physical channels exceed 512; no automatic span is displayed");
  }
  return {};
}
}
DmxPatchPanel::DmxPatchPanel(wxWindow *parent, ConfigManager &config, std::function<void()> close)
    : wxPanel(parent), config_(config) {
  SetName(_("DMX Patch"));
  auto *outer = new wxBoxSizer(wxVERTICAL);
  auto *bar = new wxWrapSizer(wxHORIZONTAL);
  bar->Add(new wxStaticText(this, wxID_ANY, _("DMX Patch")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
  bar->Add(new wxStaticText(this, wxID_ANY, _("Universe:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
  universeControl_ = new wxComboBox(this, wxID_ANY, "1", wxDefaultPosition, FromDIP(wxSize(105, -1)), 0, nullptr, wxTE_PROCESS_ENTER);
  universeControl_->SetName(_("DMX universe"));
  universeControl_->SetToolTip(_("Choose a patched universe or enter any positive universe number."));
  bar->Add(universeControl_, 0, wxRIGHT, FromDIP(6));
  auto *columns = new wxChoice(this, wxID_ANY);
  columns->Append(_("16 columns")); columns->Append(_("32 columns")); columns->SetSelection(0);
  bar->Add(columns, 0, wxRIGHT, FromDIP(6));
  status_ = new wxStaticText(this, wxID_ANY, _("DMX Patch"), wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
  status_->SetMinSize(wxSize(1, status_->GetCharHeight() + FromDIP(4)));
  auto *hide = new wxButton(this, wxID_ANY, _("Close"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  hide->Bind(wxEVT_BUTTON, [close](wxCommandEvent &) { close(); });
  bar->Add(hide, 0);
  outer->Add(bar, 0, wxEXPAND | wxALL, FromDIP(4));
  outer->Add(status_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(4));
  canvas_ = new DmxPatchCanvas(this);
  outer->Add(canvas_, 1, wxEXPAND);
  SetSizer(outer);
  columns->Bind(wxEVT_CHOICE, [this, columns](wxCommandEvent &) { canvas_->SetColumns(columns->GetSelection() ? 32 : 16); });
  universeControl_->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) { DisplayUniverse(); });
  universeControl_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) { DisplayUniverse(); });
  universeControl_->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) { DisplayUniverse(); e.Skip(); });
  subscription_ = config_.SubscribeChanges([this](ConfigManager::Change change) {
    if (change == ConfigManager::Change::Scene) { stale_ = true; QueueRefresh(); }
  });
  Bind(wxEVT_SHOW, [this](wxShowEvent &e) { if (e.IsShown() && stale_) QueueRefresh(); e.Skip(); });
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
  auto snapshot = std::make_shared<dmx_patch::Snapshot>(dmx_patch::BuildSnapshot(scene, universe_, resolver));
  if (newProject && !snapshot->universes.empty() && !snapshot->universes.contains(universe_)) {
    universe_ = *snapshot->universes.begin();
    *snapshot = dmx_patch::BuildSnapshot(scene, universe_, resolver);
  }
  for (auto it = cache_.begin(); it != cache_.end();) {
    const bool used = std::any_of(paths_.begin(), paths_.end(), [&](const auto &entry) { return entry.second == it->first; });
    if (!used) it = cache_.erase(it); else ++it;
  }
  universeControl_->Clear();
  for (auto universe : snapshot->universes) universeControl_->Append(Text(std::to_string(universe)));
  universeControl_->ChangeValue(Text(std::to_string(universe_)));
  status_->SetLabel(wxString::Format(_("Occupied: %d/512 · Conflicts: %d · Warnings: %d"),
      snapshot->occupied, snapshot->conflicts, static_cast<int>(snapshot->diagnostics.size())));
  wxString details = status_->GetLabel();
  // Keep native tooltips bounded on large incomplete projects.
  for (size_t i = 0; i < snapshot->diagnostics.size() && i < 40; ++i) {
    const auto &issue = snapshot->diagnostics[i];
    if (!details.empty()) details += "\n";
    details += Text(issue.uuid) + ": " + IssueText(issue.issue);
    if (!issue.detail.empty()) details += " (" + Text(issue.detail) + ")";
  }
  if (snapshot->diagnostics.size() > 40) details += "\n" + _("Additional diagnostics omitted.");
  status_->SetToolTip(details);
  canvas_->SetSnapshot(snapshot);
  stale_ = false;
  Layout();
}
