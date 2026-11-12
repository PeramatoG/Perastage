/* This file is part of Perastage. License: GPL-3.0-or-later. */
#pragma once
#include "patch/dmx_patch_snapshot.h"
#include "fixturetable/fixture_table_highlight_event.h"
#include <wx/panel.h>
#include <wx/weakref.h>
#include <filesystem>
#include <functional>
class ConfigManager;
class wxComboBox;
class wxStaticText;
class wxBitmapButton;
class wxCheckBox;
class DmxPatchCanvas;
class wxPopupTransientWindow;
class DmxPatchPanel : public wxPanel {
public:
  DmxPatchPanel(wxWindow *parent, ConfigManager &config);
  ~DmxPatchPanel() override;
  void RefreshSnapshot();
private:
  struct CachedDocument {
    std::filesystem::file_time_type modified;
    std::uintmax_t size = 0;
    std::shared_ptr<const gdtf::GdtfModeChannelDocument> document;
  };
  void QueueRefresh();
  void ShowDiagnostics();
  void DisplayUniverse(const wxString &value);
  void OnHostShow(wxShowEvent &event);
  void OnHoverHighlight(wxThreadEvent &event);
  void RefreshHighlights();
  std::shared_ptr<const gdtf::GdtfModeChannelDocument> Resolve(const Fixture &fixture);
  ConfigManager &config_;
  std::size_t subscription_ = 0;
  bool queued_ = false, stale_ = true;
  dmx_patch::Universe universe_ = 1;
  bool projectKnown_ = false;
  std::string projectBase_;
  std::map<std::string, CachedDocument> cache_;
  std::map<std::string, std::string> paths_;
  wxComboBox *universeControl_;
  wxStaticText *status_;
  DmxPatchCanvas *canvas_;
  wxBitmapButton *warnings_;
  wxCheckBox *compatibility_;
  std::shared_ptr<const dmx_patch::Snapshot> snapshot_;
  gui::FixtureHoverHighlight hoverHighlight_;
  wxWindow *host_ = nullptr;
  std::set<dmx_patch::Universe> listedUniverses_;
  wxWeakRef<wxPopupTransientWindow> diagnosticsPopup_;
};
