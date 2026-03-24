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
#include "app_version.h"
#include "configmanager.h"
#include "gdtfdictionary.h"
#include "logger.h"
#include "mainwindow.h"
#include "projectutils.h"
#include "splashscreen.h"
#include <filesystem>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <new>
#include <optional>
#include <string>
#include <thread>
#include <wx/stackwalk.h>
#include <wx/sysopt.h>
#include <wx/timer.h>
#include <wx/weakref.h>
#include <wx/wx.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

class MyApp : public wxApp {
public:
  virtual bool OnInit() override;
  int OnExit() override;
  int FilterEvent(wxEvent &event) override;
  bool OnExceptionInMainLoop() override;
  void OnUnhandledException() override;

private:
  void QueueProjectLoadedEvent(const wxWeakRef<MainWindow> &mainWindowRef,
                               bool loaded, bool clearLastProject,
                               const std::string &path = {});
  void ArmStartupProjectLoadTimeout(const wxWeakRef<MainWindow> &mainWindowRef);
  void CancelStartupProjectLoadTimeout();
  void OnStartupProjectLoadTimeout(wxTimerEvent &event);

  std::string last_event_summary_;
  std::thread project_loader_thread_;
  std::atomic<bool> project_load_event_sent_{false};
  wxTimer startup_project_timeout_timer_;
  wxWeakRef<MainWindow> startup_timeout_main_window_ref_;
};

namespace {
std::string ToLowerAscii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

std::optional<std::string> GetStartupPathFromArgs(int argc, wxChar **argv) {
  namespace fs = std::filesystem;
  const std::string projectExtension = ToLowerAscii(ProjectUtils::PROJECT_EXTENSION);
  for (int i = 1; i < argc; ++i) {
    const std::string rawPath = wxString(argv[i]).ToStdString();
    if (rawPath.empty())
      continue;

    const fs::path candidate = fs::u8path(rawPath);
    const std::u8string extensionU8 = candidate.extension().u8string();
    const std::string extension(extensionU8.begin(), extensionU8.end());
    const std::string normalizedExtension = ToLowerAscii(extension);
    if (normalizedExtension != projectExtension && normalizedExtension != ".mvr")
      continue;

    std::error_code ec;
    fs::path absolutePath = fs::absolute(candidate, ec);
    if (ec)
      return rawPath;
    const std::u8string absoluteU8 = absolutePath.u8string();
    if (absoluteU8.empty())
      return rawPath;
    return std::string(absoluteU8.begin(), absoluteU8.end());
  }
  return std::nullopt;
}

class GdtfDictionaryStartupDialog : public wxDialog {
public:
  GdtfDictionaryStartupDialog(wxWindow *parent, const std::string &fallbackPath)
      : wxDialog(parent, wxID_ANY, "Dictionary access issue",
                 wxDefaultPosition, wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
        fallbackPath_(fallbackPath) {
    wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
    infoLabel_ = new wxStaticText(this, wxID_ANY, "");
    infoLabel_->Wrap(560);
    topSizer->Add(infoLabel_, 1, wxALL | wxEXPAND, 12);

    wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    continueButton_ = new wxButton(this, wxID_CANCEL, "Continue read-only");
    retryButton_ = new wxButton(this, wxID_REFRESH, "Retry");
    buttonSizer->Add(continueButton_, 0, wxRIGHT, 8);
    buttonSizer->Add(retryButton_, 0);
    topSizer->Add(buttonSizer, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM,
                  12);

    SetSizerAndFit(topSizer);
    SetMinSize(wxSize(640, 260));

    continueButton_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Destroy(); });
    retryButton_->Bind(wxEVT_BUTTON,
                       &GdtfDictionaryStartupDialog::OnRetry, this);
  }

  void UpdateIssue(const GdtfDictionary::AccessIssue &issue) {
    const wxString message = wxString::Format(
        "The application could not access the GDTF dictionary.\n\n"
        "Attempted path:\n%s\n\n"
        "Operation: %s\n"
        "Cause: %s\n\n"
        "Suggestion: move the dictionary to your user fixtures folder:\n%s\n\n"
        "You can continue in read-only mode or retry after fixing permissions/path.",
        wxString::FromUTF8(issue.attemptedPath), wxString::FromUTF8(issue.operation),
        wxString::FromUTF8(issue.cause), wxString::FromUTF8(fallbackPath_));
    infoLabel_->SetLabel(message);
    Layout();
  }

private:
  void OnRetry(wxCommandEvent &) {
    auto dict = GdtfDictionary::Load();
    if (dict) {
      Destroy();
      return;
    }
    const GdtfDictionary::AccessIssue issue =
        GdtfDictionary::ConsumeLastAccessIssue().value_or(
            GdtfDictionary::AccessIssue{
                "", "open/read",
                "dictionary is still unavailable after retry"});
    UpdateIssue(issue);
  }

  wxStaticText *infoLabel_ = nullptr;
  wxButton *continueButton_ = nullptr;
  wxButton *retryButton_ = nullptr;
  std::string fallbackPath_;
};

void ShowGdtfDictionaryStartupIssue(wxWindow *parent) {
  // Startup dictionary search anchor: "gdtf_dictionary.json" and GdtfDictionary::Load.
  auto issueOpt = GdtfDictionary::ConsumeLastAccessIssue();
  if (!issueOpt)
    return;
  const std::string fallbackPath =
      ProjectUtils::GetDefaultLibraryPath("fixtures");
  Logger::Instance().Log(
      Logger::Level::Warn,
      "Starting with non-editable GDTF dictionary (controlled degradation).");
  auto *dialog = new GdtfDictionaryStartupDialog(parent, fallbackPath);
  dialog->UpdateIssue(*issueOpt);
  dialog->Show();
}
} // namespace

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit() {
  SetAppName(app::kName);
  SetVendorName("Perasoft");

  // Ensure a stable working directory when launching from Explorer.
  // This avoids issues caused by relative paths during startup/project load.
  {
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    if (exe.IsOk()) {
      wxFileName::SetCwd(exe.GetPath());
    }
  }

  // Enable support for common image formats used by the app
  wxInitAllImageHandlers();

  // Force dark mode when supported by the wxWidgets version in use
#if wxCHECK_VERSION(3, 3, 0)
  SetAppearance(wxApp::Appearance::Dark);
#endif

  // Enable dark mode for Windows (if supported by wxWidgets)
  wxSystemOptions::SetOption("msw.useDarkMode", 1);

  SplashScreen::Show();
  SplashScreen::SetMessage("Initializing logger...");

  // Initialize logging system (overwrites log file each launch)
  Logger::Instance();

  SplashScreen::SetMessage("Creating main window...");
  MainWindow *mainWindow = new MainWindow(app::kName);
  mainWindow->Show(true);
  // Start maximized so minimize and restore buttons remain available
  mainWindow->Maximize(true);

  if (!GdtfDictionary::Load()) {
    ShowGdtfDictionaryStartupIssue(mainWindow);
  }

  const std::optional<std::string> startupPathOpt =
      GetStartupPathFromArgs(argc, argv);

  SplashScreen::SetMessage("Loading last project...");
  wxWeakRef<MainWindow> mainWindowRef(mainWindow);
  auto lastPathOpt = ProjectUtils::LoadLastProjectPath();

  if (startupPathOpt && mainWindowRef) {
    QueueProjectLoadedEvent(mainWindowRef, false, false);
    const std::string startupPath = *startupPathOpt;
    mainWindow->CallAfter([mainWindowRef, startupPath]() {
      if (!mainWindowRef)
        return;
      mainWindowRef->OpenPathFromCommandLine(startupPath);
    });
  } else if (lastPathOpt) {
    ArmStartupProjectLoadTimeout(mainWindowRef);
    std::string lastPath = *lastPathOpt;
    project_loader_thread_ = std::thread([this, mainWindowRef, lastPath]() {
      try {
        namespace fs = std::filesystem;
        bool loaded = false;
        bool clearLastProject = false;
        std::string path = lastPath;
        std::error_code ec;
        fs::path lastFsPath = fs::u8path(path);
        bool isFile = fs::is_regular_file(lastFsPath, ec);
        if (ec || !isFile) {
          clearLastProject = true;
          path.clear();
        } else {
          loaded = ConfigManager::Get().LoadProject(path);
          if (!loaded)
            clearLastProject = true;
        }
        this->QueueProjectLoadedEvent(mainWindowRef, loaded, clearLastProject,
                                      path);
      } catch (const std::exception &ex) {
        Logger::Instance().Log(
            std::string("Failed to load last project: ") + ex.what());
        this->QueueProjectLoadedEvent(mainWindowRef, false, true);
      } catch (...) {
        Logger::Instance().Log("Failed to load last project: unknown error.");
        this->QueueProjectLoadedEvent(mainWindowRef, false, true);
      }
    });

  } else if (mainWindowRef) {
    QueueProjectLoadedEvent(mainWindowRef, false, false);
  }

  return true;
}

int MyApp::OnExit() {
  CancelStartupProjectLoadTimeout();
  if (project_loader_thread_.joinable()) {
    project_loader_thread_.join();
  }
  return wxApp::OnExit();
}

void MyApp::QueueProjectLoadedEvent(const wxWeakRef<MainWindow> &mainWindowRef,
                                    bool loaded, bool clearLastProject,
                                    const std::string &path) {
  if (!mainWindowRef)
    return;
  bool expected = false;
  if (!project_load_event_sent_.compare_exchange_strong(expected, true))
    return;
  CancelStartupProjectLoadTimeout();

  wxCommandEvent evt(EVT_PROJECT_LOADED);
  evt.SetInt(loaded ? 1 : 0);
  evt.SetExtraLong(clearLastProject ? 1 : 0);
  evt.SetString(wxString::FromUTF8(path));
  wxQueueEvent(mainWindowRef.get(), evt.Clone());
}

void MyApp::ArmStartupProjectLoadTimeout(
    const wxWeakRef<MainWindow> &mainWindowRef) {
  startup_timeout_main_window_ref_ = mainWindowRef;
  if (!startup_project_timeout_timer_.GetOwner()) {
    constexpr int kStartupProjectTimeoutTimerId = wxID_HIGHEST + 712;
    startup_project_timeout_timer_.SetOwner(this, kStartupProjectTimeoutTimerId);
    Bind(wxEVT_TIMER, &MyApp::OnStartupProjectLoadTimeout, this,
         kStartupProjectTimeoutTimerId);
  }
  constexpr int kStartupProjectLoadTimeoutMs = 12000;
  startup_project_timeout_timer_.StartOnce(kStartupProjectLoadTimeoutMs);
}

void MyApp::CancelStartupProjectLoadTimeout() {
  if (startup_project_timeout_timer_.IsRunning())
    startup_project_timeout_timer_.Stop();
}

void MyApp::OnStartupProjectLoadTimeout(wxTimerEvent &WXUNUSED(event)) {
  bool expected = false;
  if (!project_load_event_sent_.compare_exchange_strong(expected, true))
    return;

  Logger::Instance().Log(
      Logger::Level::Warn,
      "Startup last-project load timed out after 12000ms. Continuing without loading the last project.");
  if (!startup_timeout_main_window_ref_)
    return;
  wxCommandEvent evt(EVT_PROJECT_LOADED);
  evt.SetInt(0);
  evt.SetExtraLong(1);
  evt.SetString("");
  wxQueueEvent(startup_timeout_main_window_ref_.get(), evt.Clone());
}

int MyApp::FilterEvent(wxEvent &event) {
  const wxClassInfo *eventInfo = event.GetClassInfo();
  wxString eventClassName =
      eventInfo ? eventInfo->GetClassName() : wxT("UnknownEvent");
  wxString objectClassName = wxT("None");
  if (event.GetEventObject()) {
    const wxClassInfo *objectInfo = event.GetEventObject()->GetClassInfo();
    if (objectInfo) {
      objectClassName = objectInfo->GetClassName();
    } else {
      objectClassName = wxT("UnknownObject");
    }
  }
  last_event_summary_ = wxString::Format(
                            "Last event: class=%s type=%d id=%d object=%s",
                            eventClassName, static_cast<int>(event.GetEventType()),
                            event.GetId(), objectClassName)
                            .ToStdString();
  return -1;
}

namespace {
void LogExceptionWithStack(const std::exception &ex,
                           const char *contextMessage) {
  Logger::Instance().Log(std::string(contextMessage) + ex.what());

#if defined(__WXMSW__)
  class StackWalker : public wxStackWalker {
  public:
    std::string TakeStackTrace() {
      lines_.clear();
      Walk();
      return lines_;
    }

  protected:
    void OnStackFrame(const wxStackFrame &frame) override {
      lines_ += std::string(frame.GetName().ToStdString());
      lines_ += " (";
      lines_ += std::string(frame.GetFileName().ToStdString());
      lines_ += ":";
      lines_ += std::to_string(frame.GetLine());
      lines_ += ")\n";
    }

  private:
    std::string lines_;
  };

  StackWalker walker;
  std::string trace = walker.TakeStackTrace();
  if (!trace.empty()) {
    Logger::Instance().Log(std::string("Stack trace:\n") + trace);
  }
#endif
}
} // namespace

bool MyApp::OnExceptionInMainLoop() {
  try {
    throw;
  } catch (const std::exception &ex) {
    if (dynamic_cast<const std::bad_alloc *>(&ex)) {
      Logger::Instance().Log("Unhandled exception in main loop: bad allocation.");
      if (!last_event_summary_.empty()) {
        Logger::Instance().Log(last_event_summary_);
      }
      LogExceptionWithStack(ex, "Unhandled exception in main loop: ");
      return true;
    }
    if (!last_event_summary_.empty()) {
      Logger::Instance().Log(last_event_summary_);
    }
    LogExceptionWithStack(ex, "Unhandled exception in main loop: ");
    return true;
  } catch (...) {
    Logger::Instance().Log("Unhandled exception in main loop: unknown error.");
  }
  return false;
}

void MyApp::OnUnhandledException() {
  try {
    throw;
  } catch (const std::exception &ex) {
    if (dynamic_cast<const std::bad_alloc *>(&ex)) {
      Logger::Instance().Log("Unhandled exception: bad allocation.");
      if (!last_event_summary_.empty()) {
        Logger::Instance().Log(last_event_summary_);
      }
      return;
    }
    if (!last_event_summary_.empty()) {
      Logger::Instance().Log(last_event_summary_);
    }
    LogExceptionWithStack(ex, "Unhandled exception: ");
  } catch (...) {
    Logger::Instance().Log("Unhandled exception: unknown error.");
  }
}
