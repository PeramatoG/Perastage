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
#include "build_info.h"
#include "filesystem_path_utils.h"
#include "localization/localization_manager.h"
#include "app_version.h"
#include "configmanager.h"
#include "diagnostics/CrashHandler.h"
#include "diagnostics/DiagnosticLogger.h"
#include "mainwindow.h"
#include "platform_locale.h"
#include "projectutils.h"
#include "splashscreen.h"
#include <filesystem>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <deque>
#include <new>
#include <optional>
#include <string>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif
#include <wx/stackwalk.h>
#include <wx/sysopt.h>
#include <wx/uri.h>
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

  std::string last_event_summary_;
  std::atomic<bool> project_load_event_sent_{false};
  std::atomic<bool> startup_resolution_pending_{true};
  std::optional<std::string> explicit_startup_open_path_;
  std::deque<std::string> pending_external_open_paths_;
};

namespace {
// Converts a file:// URI into a local path while handling encoded characters and localhost-style authorities.
wxString DecodeFileUriToPath(const wxString &fileUri) {
  wxURI uri(fileUri);
  if (!uri.IsReference() && uri.GetScheme().CmpNoCase("file") == 0 &&
      uri.HasPath()) {
    wxString path = wxURI::Unescape(uri.GetPath());
#if defined(__WXOSX__)
    if (path.StartsWith("//")) {
      while (path.StartsWith("//"))
        path = path.Mid(1);
      path.Prepend("/");
    }
#endif
    if (!path.empty())
      return path;
  }
  return fileUri;
}

// Converts external open input into a UTF-8 path while avoiding Windows shell-folder normalization stalls.
std::string NormalizeExternalOpenPath(const std::string &rawPathUtf8) {
  if (rawPathUtf8.empty())
    return {};

  wxString wxPath = wxString::FromUTF8(rawPathUtf8);
  if (wxPath.StartsWith("file://"))
    wxPath = DecodeFileUriToPath(wxPath);

#if defined(__WXMSW__)
  wxPath.Replace("/", "\\");
#else
  wxFileName fileName(wxPath);
  if (fileName.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_TILDE |
                         wxPATH_NORM_ABSOLUTE)) {
    wxPath = fileName.GetFullPath();
  }
#endif

  wxCharBuffer utf8 = wxPath.ToUTF8();
  const std::string normalizedPath =
      utf8 ? std::string(utf8.data()) : rawPathUtf8;
  if (normalizedPath != rawPathUtf8) {
    diagnostics::DiagnosticLogger::Debug(
        "NormalizeExternalOpenPath normalized filename '" +
        diagnostics::DiagnosticLogger::FileNameOnly(rawPathUtf8) + "' -> '" +
        diagnostics::DiagnosticLogger::FileNameOnly(normalizedPath) + "'");
  }
  return normalizedPath;
}

// Converts an ASCII string to lowercase.
std::string ToLowerAscii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

// Returns the first startup-open candidate from CLI arguments as a normalized absolute UTF-8 path.
std::optional<std::string> GetStartupPathFromArgs(
    int argc, wxChar **argv, const std::string &launchWorkingDirectoryUtf8) {
  namespace fs = std::filesystem;
  const std::string projectExtension = ToLowerAscii(ProjectUtils::PROJECT_EXTENSION);
  const fs::path launchWorkingDirectory =
      launchWorkingDirectoryUtf8.empty()
          ? fs::path()
          : PathUtils::PathFromUtf8(launchWorkingDirectoryUtf8);

  auto toUtf8 = [](const wxString &text) {
    wxCharBuffer utf8 = text.ToUTF8();
    if (!utf8)
      return text.ToStdString();
    return std::string(utf8.data());
  };

  for (int i = 1; i < argc; ++i) {
    wxString argumentText(argv[i]);
    if ((argumentText.StartsWith("\"") && argumentText.EndsWith("\"")) ||
        (argumentText.StartsWith("'") && argumentText.EndsWith("'"))) {
      argumentText = argumentText.Mid(1, argumentText.length() - 2);
    }

    const std::string rawPath = toUtf8(argumentText);
    if (rawPath.empty())
      continue;

    const std::string normalizedRawPath = NormalizeExternalOpenPath(rawPath);
    const fs::path candidate = PathUtils::PathFromUtf8(normalizedRawPath);
    const std::u8string extensionU8 = candidate.extension().u8string();
    const std::string extension(extensionU8.begin(), extensionU8.end());
    const std::string normalizedExtension = ToLowerAscii(extension);
    if (normalizedExtension != projectExtension && normalizedExtension != ".mvr")
      continue;

    std::error_code ec;
    fs::path absolutePath;
    if (!candidate.is_absolute() && !launchWorkingDirectory.empty()) {
      absolutePath = fs::absolute(launchWorkingDirectory / candidate, ec);
    } else {
      absolutePath = fs::absolute(candidate, ec);
    }
    if (ec)
      return normalizedRawPath;
    const std::u8string absoluteU8 = absolutePath.u8string();
    if (absoluteU8.empty())
      return normalizedRawPath;
    return std::string(absoluteU8.begin(), absoluteU8.end());
  }
  return std::nullopt;
}

#if defined(_MSC_VER) && defined(_DEBUG)
// Configures optional CRT leak checking for Windows debug builds via an environment toggle.
void ConfigureWindowsDebugHeapLeakCheck() {
  int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
  const char *requestedLeakCheck = std::getenv("PERASTAGE_CRT_LEAK_CHECK");
  const bool enableLeakCheck =
      requestedLeakCheck && std::string(requestedLeakCheck) == "1";

  if (enableLeakCheck) {
    flags |= _CRTDBG_LEAK_CHECK_DF;
  } else {
    flags &= ~_CRTDBG_LEAK_CHECK_DF;
  }

  _CrtSetDbgFlag(flags);
}
#endif
} // namespace

wxIMPLEMENT_APP(MyApp);

// Initializes the application, creates the main window, and routes startup open requests.
bool MyApp::OnInit() {
#if defined(_MSC_VER) && defined(_DEBUG)
  ConfigureWindowsDebugHeapLeakCheck();
#endif

  const platform::LocaleSetupResult localeSetup =
      platform::EnsureProcessTextLocale();

  const wxString launchCwdWx = wxFileName::GetCwd();
  const wxCharBuffer launchCwdUtf8Buffer = launchCwdWx.ToUTF8();
  const std::string launchWorkingDirectoryUtf8 =
      launchCwdUtf8Buffer ? std::string(launchCwdUtf8Buffer.data())
                          : launchCwdWx.ToStdString();

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

  // Initialize logging and crash reporting before creating user-facing windows.
  diagnostics::DiagnosticLogger::Initialize();
  diagnostics::CrashHandler::Initialize();
  diagnostics::DiagnosticLogger::Info(
      std::string("Perastage startup: version=") +
      std::string(perastage::build_info::appVersionDisplay()) +
      " commit=" + std::string(perastage::build_info::gitCommit()) +
      " build_type=" +
      std::string(perastage::build_info::buildConfiguration()) +
      " platform=" + std::string(perastage::build_info::targetPlatform()) +
      " text_locale=" + localeSetup.activeLocale);
  if (!localeSetup.note.empty())
    diagnostics::DiagnosticLogger::Warning("Startup text locale: " + localeSetup.note);

  // Load preferences before UI localization; localization preserves LC_NUMERIC for technical data.
  ConfigManager &config = ConfigManager::Get();
  const localization::AppLanguage requestedLanguage =
      localization::ParseAppLanguageCode(
          config.GetValue(localization::kUiLanguageConfigKey).value_or(""));
  const localization::LocalizationInitResult localizationResult =
      localization::LocalizationManager::Get().Initialize(requestedLanguage);
  if (!localizationResult.diagnostic.empty() &&
      localizationResult.activeLanguage != localizationResult.requestedLanguage) {
    diagnostics::DiagnosticLogger::Warning("Startup localization: " +
                                          localizationResult.diagnostic);
  }

  SplashScreen::Show();
  SplashScreen::SetMessage("Running library bootstrap...");
  ProjectUtils::RunStartupLibraryBootstrap();

  SplashScreen::SetMessage("Creating main window...");
  MainWindow *mainWindow = new MainWindow(app::kName);
  mainWindow->Show(true);
  // Start maximized so minimize and restore buttons remain available
  mainWindow->Maximize(true);

  SplashScreen::SetMessage("Loading last project...");
  wxWeakRef<MainWindow> mainWindowRef(mainWindow);
  auto lastPathOpt = ProjectUtils::LoadLastProjectPath();
  std::optional<std::string> cliStartupPath =
      GetStartupPathFromArgs(argc, argv, launchWorkingDirectoryUtf8);
  diagnostics::DiagnosticLogger::Info(
      "Startup resolution delayed to allow macOS open-file event.");
  mainWindow->CallAfter([this, mainWindowRef, cliStartupPath, lastPathOpt]() {
    if (!mainWindowRef)
      return;
    mainWindowRef->CallAfter([this, mainWindowRef, cliStartupPath, lastPathOpt]() {
      FinalizeStartupOpenResolution(mainWindowRef, cliStartupPath, lastPathOpt);
    });
  });
  return true;
}

// Finalizes startup path selection after allowing macOS open-file events to arrive.
void MyApp::FinalizeStartupOpenResolution(
    const wxWeakRef<MainWindow> &mainWindowRef,
    const std::optional<std::string> &cliStartupPath,
    const std::optional<std::string> &lastPathOpt) {
  if (!mainWindowRef)
    return;

  if (auto macPath = ConsumePendingStartupExternalOpenPath()) {
    diagnostics::DiagnosticLogger::Info("Startup explicit macOS path selected: " + diagnostics::DiagnosticLogger::FileNameOnly(*macPath));
    diagnostics::DiagnosticLogger::Info("Opening startup path: " + diagnostics::DiagnosticLogger::FileNameOnly(*macPath));
    QueueProjectLoadedEvent(mainWindowRef, false, true, *macPath);
    startup_resolution_pending_.store(false);
    return;
  }

  if (cliStartupPath.has_value()) {
    diagnostics::DiagnosticLogger::Info("Startup CLI path selected: " + diagnostics::DiagnosticLogger::FileNameOnly(*cliStartupPath));
    diagnostics::DiagnosticLogger::Info("Opening startup path: " + diagnostics::DiagnosticLogger::FileNameOnly(*cliStartupPath));
    QueueProjectLoadedEvent(mainWindowRef, false, true, *cliStartupPath);
    startup_resolution_pending_.store(false);
    return;
  }

  if (lastPathOpt.has_value()) {
    if (explicit_startup_open_path_.has_value()) {
      diagnostics::DiagnosticLogger::Info(
          "Skipping last project because explicit startup path exists.");
    } else {
      diagnostics::DiagnosticLogger::Info("Startup last project selected: " + diagnostics::DiagnosticLogger::FileNameOnly(*lastPathOpt));
      QueueProjectLoadedEvent(mainWindowRef, false, false, *lastPathOpt);
    }
    startup_resolution_pending_.store(false);
    return;
  }

  diagnostics::DiagnosticLogger::Info("Startup empty project selected.");
  QueueProjectLoadedEvent(mainWindowRef, false, false);
  startup_resolution_pending_.store(false);
}

// Routes a single macOS file-open request to the shared external-open handler.
void MyApp::MacOpenFile(const wxString &fileName) {
  const wxCharBuffer utf8 = fileName.ToUTF8();
  const std::string rawPathUtf8 =
      utf8 ? std::string(utf8.data()) : fileName.ToStdString();
  diagnostics::DiagnosticLogger::Info("MacOpenFile received: " + diagnostics::DiagnosticLogger::FileNameOnly(rawPathUtf8));
  HandleExternalOpenPath(NormalizeExternalOpenPath(rawPathUtf8));
}

// Routes macOS multi-file open requests to the external open pipeline in order.
void MyApp::MacOpenFiles(const wxArrayString &fileNames) {
  diagnostics::DiagnosticLogger::Info("MacOpenFiles received count: " +
                         std::to_string(fileNames.GetCount()));
  for (const wxString &fileName : fileNames) {
    MacOpenFile(fileName);
  }
}


// Routes macOS URL open requests (Finder/LaunchServices) to the shared external-open handler.
void MyApp::MacOpenURL(const wxString &url) {
  const wxCharBuffer utf8 = url.ToUTF8();
  const std::string rawUrlUtf8 =
      utf8 ? std::string(utf8.data()) : url.ToStdString();
  diagnostics::DiagnosticLogger::Info("MacOpenURL received.");
  HandleExternalOpenPath(NormalizeExternalOpenPath(rawUrlUtf8));
}

// Handles external file-open requests by deferring them until startup loading is safe.
void MyApp::HandleExternalOpenPath(const std::string &pathUtf8) {
  if (pathUtf8.empty())
    return;
  diagnostics::DiagnosticLogger::Info("macOS external open received: " + diagnostics::DiagnosticLogger::FileNameOnly(pathUtf8));

  if (startup_resolution_pending_.load()) {
    StorePendingStartupExternalOpenPath(pathUtf8);
    diagnostics::DiagnosticLogger::Info("macOS startup explicit path stored: " + diagnostics::DiagnosticLogger::FileNameOnly(pathUtf8));
    return;
  }

  MainWindow *mainWindow = wxDynamicCast(GetTopWindow(), MainWindow);
  if (!mainWindow) {
    diagnostics::DiagnosticLogger::Info(
        "HandleExternalOpenPath queued: MainWindow is not ready.");
    pending_external_open_paths_.push_back(pathUtf8);
    return;
  }

  if (!project_load_event_sent_.load()) {
    diagnostics::DiagnosticLogger::Info("Opening startup path: " + diagnostics::DiagnosticLogger::FileNameOnly(pathUtf8));
    QueueProjectLoadedEvent(wxWeakRef<MainWindow>(mainWindow), false, true, pathUtf8);
    return;
  }

  diagnostics::DiagnosticLogger::Info(
      "HandleExternalOpenPath routing through MainWindow deferred-open pipeline.");
  mainWindow->CallAfter([mainWindow, pathUtf8]() {
    if (!mainWindow->CanProcessExternalOpenPath())
      return;
    mainWindow->EnqueueExternalOpenPath(pathUtf8);
  });
}

// Stores the most recent explicit startup external-open path received during app initialization.
void MyApp::StorePendingStartupExternalOpenPath(const std::string &pathUtf8) {
  if (pathUtf8.empty())
    return;
  explicit_startup_open_path_ = pathUtf8;
}

// Returns and clears the explicit startup external-open path captured during initialization.
std::optional<std::string> MyApp::ConsumePendingStartupExternalOpenPath() {
  if (!explicit_startup_open_path_.has_value())
    return std::nullopt;
  std::optional<std::string> path = explicit_startup_open_path_;
  explicit_startup_open_path_.reset();
  return path;
}

// Pops and returns the next queued external-open path, if any.
std::optional<std::string> MyApp::ConsumePendingExternalOpenPath() {
  if (pending_external_open_paths_.empty())
    return std::nullopt;
  std::string path = pending_external_open_paths_.front();
  pending_external_open_paths_.pop_front();
  return path;
}

// Releases application-level resources before process shutdown.
int MyApp::OnExit() {
  diagnostics::DiagnosticLogger::ShutdownForExit("Perastage shutdown started.");
  // Release application-level singletons before CRT leak reporting runs in
  // debug builds on Windows.
  ConfigManager::Get().Reset();
  ConfigManager::Get().ClearHistory();
  wxImage::CleanUpHandlers();
  return wxApp::OnExit();
}

// Queues a single startup project-loaded event to avoid duplicate startup routing.
void MyApp::QueueProjectLoadedEvent(const wxWeakRef<MainWindow> &mainWindowRef,
                                    bool loaded, bool clearLastProject,
                                    const std::string &path) {
  if (!mainWindowRef)
    return;
  bool expected = false;
  if (!project_load_event_sent_.compare_exchange_strong(expected, true))
    return;

  wxCommandEvent evt(EVT_PROJECT_LOADED);
  evt.SetInt(loaded ? 1 : 0);
  evt.SetExtraLong(clearLastProject ? 1 : 0);
  evt.SetString(wxString::FromUTF8(path));
  wxQueueEvent(mainWindowRef.get(), evt.Clone());
}

// Captures the latest event metadata to improve crash diagnostics.
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
  last_event_summary_ =
      ("Last event: class=" + eventClassName +
       wxString::Format(" type=%d id=%d object=",
                        static_cast<int>(event.GetEventType()), event.GetId()) +
       objectClassName)
          .ToStdString();
  return -1;
}

namespace {
// Logs an exception message and an optional platform stack trace for diagnostics.
void LogExceptionWithStack(const std::exception &ex,
                           const char *contextMessage) {
  diagnostics::DiagnosticLogger::Error(std::string(contextMessage) + ex.what());

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
    diagnostics::DiagnosticLogger::Error(std::string("Stack trace:\n") + trace);
  }
#endif
}
} // namespace

// Logs recoverable main-loop exceptions and keeps the app running when possible.
bool MyApp::OnExceptionInMainLoop() {
  try {
    throw;
  } catch (const std::exception &ex) {
    if (dynamic_cast<const std::bad_alloc *>(&ex)) {
      diagnostics::DiagnosticLogger::Error("Unhandled exception in main loop: bad allocation.");
      if (!last_event_summary_.empty()) {
        diagnostics::DiagnosticLogger::Error(last_event_summary_);
      }
      LogExceptionWithStack(ex, "Unhandled exception in main loop: ");
      diagnostics::CrashHandler::WriteExceptionReport(
          "Unhandled exception in main loop", last_event_summary_);
      return true;
    }
    if (!last_event_summary_.empty()) {
      diagnostics::DiagnosticLogger::Error(last_event_summary_);
    }
    LogExceptionWithStack(ex, "Unhandled exception in main loop: ");
    diagnostics::CrashHandler::WriteExceptionReport(
        "Unhandled exception in main loop", last_event_summary_);
    return true;
  } catch (...) {
    diagnostics::DiagnosticLogger::Error("Unhandled exception in main loop: unknown error.");
    diagnostics::CrashHandler::WriteExceptionReport(
        "Unhandled exception in main loop", last_event_summary_);
  }
  return false;
}

// Logs non-recoverable unhandled exceptions for post-mortem diagnostics.
void MyApp::OnUnhandledException() {
  try {
    throw;
  } catch (const std::exception &ex) {
    if (dynamic_cast<const std::bad_alloc *>(&ex)) {
      diagnostics::DiagnosticLogger::Error("Unhandled exception: bad allocation.");
      if (!last_event_summary_.empty()) {
        diagnostics::DiagnosticLogger::Error(last_event_summary_);
      }
      diagnostics::CrashHandler::WriteExceptionReport("Unhandled exception",
                                                      last_event_summary_);
      return;
    }
    if (!last_event_summary_.empty()) {
      diagnostics::DiagnosticLogger::Error(last_event_summary_);
    }
    LogExceptionWithStack(ex, "Unhandled exception: ");
    diagnostics::CrashHandler::WriteExceptionReport("Unhandled exception",
                                                    last_event_summary_);
  } catch (...) {
    diagnostics::DiagnosticLogger::Error("Unhandled exception: unknown error.");
    diagnostics::CrashHandler::WriteExceptionReport("Unhandled exception",
                                                      last_event_summary_);
  }
}
