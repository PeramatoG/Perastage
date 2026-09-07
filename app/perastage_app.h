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
#ifndef PERASTAGE_APP_H
#define PERASTAGE_APP_H

#include "localization/localization_manager.h"
#include "startup_profile.h"
#include <atomic>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <wx/app.h>
#include <wx/arrstr.h>
#include <wx/weakref.h>

class MainWindow;

class MyApp : public wxApp {
public:
  virtual bool OnInit() override;
  int OnExit() override;
  int FilterEvent(wxEvent &event) override;
  bool OnExceptionInMainLoop() override;
  void OnUnhandledException() override;
#if defined(__WXOSX__)
  void MacOpenFiles(const wxArrayString &fileNames) override;
  void MacOpenFile(const wxString &fileName) override;
  void MacOpenURL(const wxString &url) override;
#else
  void MacOpenFiles(const wxArrayString &fileNames);
  void MacOpenFile(const wxString &fileName);
  void MacOpenURL(const wxString &url);
#endif

private:
  void FinalizeStartupOpenResolution(
      const wxWeakRef<MainWindow> &mainWindowRef,
      const std::optional<std::string> &cliStartupPath,
      const std::optional<std::string> &lastPathOpt);
  void HandleExternalOpenPath(const std::string &pathUtf8);
  void StorePendingStartupExternalOpenPath(const std::string &pathUtf8);
  std::optional<std::string> ConsumePendingStartupExternalOpenPath();
  std::optional<std::string> ConsumePendingExternalOpenPath();
  void QueueProjectLoadedEvent(const wxWeakRef<MainWindow> &mainWindowRef,
                               bool loaded, bool clearLastProject,
                               const std::string &path = {});
  void ShowLocalizationFallbackWarningIfNeeded(
      const std::string &configuredLanguageCode,
      localization::AppLanguage activeLanguage);

  std::string last_event_summary_;
  std::atomic<bool> project_load_event_sent_{false};
  std::atomic<bool> startup_resolution_pending_{true};
  std::optional<std::string> explicit_startup_open_path_;
  std::deque<std::string> pending_external_open_paths_;
  bool localization_fallback_warning_shown_ = false;
  std::shared_ptr<startup::Metrics> startup_metrics_;
  startup::Metrics::Clock::time_point startup_resolution_started_at_;
};

#endif // PERASTAGE_APP_H
