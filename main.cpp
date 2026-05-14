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
#include "logger.h"
#include "mainwindow.h"
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
  void HandleExternalOpenPath(const std::string &pathUtf8);
  std::optional<std::string> ConsumePendingExternalOpenPath();
  void QueueProjectLoadedEvent(const wxWeakRef<MainWindow> &mainWindowRef,
                               bool loaded, bool clearLastProject,
                               const std::string &path = {});

  std::string last_event_summary_;
  std::atomic<bool> project_load_event_sent_{false};
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

// Converts macOS file URLs or raw paths into a normalized UTF-8 filesystem path when possible.
std::string NormalizeExternalOpenPath(const std::string &rawPathUtf8) {
  if (rawPathUtf8.empty())
    return {};

  wxString wxPath = wxString::FromUTF8(rawPathUtf8);
  if (wxPath.StartsWith("file://"))
    wxPath = DecodeFileUriToPath(wxPath);

  wxFileName fileName(wxPath);
  if (fileName.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_TILDE |
                         wxPATH_NORM_ABSOLUTE)) {
    wxPath = fileName.GetFullPath();
  }

  wxCharBuffer utf8 = wxPath.ToUTF8();
  const std::string normalizedPath =
      utf8 ? std::string(utf8.data()) : rawPathUtf8;
  if (normalizedPath != rawPathUtf8) {
    Logger::Instance().Log("NormalizeExternalOpenPath normalized '" +
                           rawPathUtf8 + "' -> '" + normalizedPath + "'");
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

// Converts wxString to UTF-8 deterministically for filesystem usage without locale-dependent fallbacks.
std::string WxStringToDeterministicUtf8Path(const wxString &text) {
  wxCharBuffer utf8 = text.ToUTF8();
  if (utf8)
    return std::string(utf8.data());

  const wxWCharBuffer wideBuffer = text.wc_str();
  const wchar_t *wideChars = wideBuffer.data();
  if (!wideChars)
    return {};

  const std::filesystem::path widePath(wideChars);
  const std::u8string utf8Path = widePath.u8string();
  return std::string(utf8Path.begin(), utf8Path.end());
}

// Returns the first startup-open candidate from CLI arguments as a normalized absolute UTF-8 path.
std::optional<std::string> GetStartupPathFromArgs(
    int argc, wxChar **argv, const std::string &launchWorkingDirectoryUtf8) {
  namespace fs = std::filesystem;
  const std::string projectExtension = ToLowerAscii(ProjectUtils::PROJECT_EXTENSION);
  const fs::path launchWorkingDirectory =
      launchWorkingDirectoryUtf8.empty()
          ? fs::path()
          : fs::u8path(launchWorkingDirectoryUtf8);

  for (int i = 1; i < argc; ++i) {
    const wxString rawArgvText(argv[i]);
    const std::string rawArgvToken = WxStringToDeterministicUtf8Path(rawArgvText);
    wxString argumentText(rawArgvText);
    if ((argumentText.StartsWith("\"") && argumentText.EndsWith("\"")) ||
        (argumentText.StartsWith("'") && argumentText.EndsWith("'"))) {
      argumentText = argumentText.Mid(1, argumentText.length() - 2);
    }

    const std::string rawPath = WxStringToDeterministicUtf8Path(argumentText);
    if (rawPath.empty())
      continue;

    const std::string normalizedRawPath = NormalizeExternalOpenPath(rawPath);
    const fs::path candidate = fs::u8path(normalizedRawPath);
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
    if (ec) {
      Logger::Instance().Log("GetStartupPathFromArgs candidate raw argv='" +
                             rawArgvToken + "' normalized absolute='" +
                             normalizedRawPath + "' (absolute failed)");
      return normalizedRawPath;
    }
    const std::u8string absoluteU8 = absolutePath.u8string();
    if (absoluteU8.empty()) {
      Logger::Instance().Log("GetStartupPathFromArgs candidate raw argv='" +
                             rawArgvToken + "' normalized absolute='" +
                             normalizedRawPath + "' (empty absolute)");
      return normalizedRawPath;
    }

    const std::string normalizedAbsolutePath(absoluteU8.begin(),
                                             absoluteU8.end());
    Logger::Instance().Log("GetStartupPathFromArgs candidate raw argv='" +
                           rawArgvToken + "' normalized absolute='" +
                           normalizedAbsolutePath + "'");
    return normalizedAbsolutePath;
  }
  return std::nullopt;
}

#if defined(_MSC_VER) && defined(_DEBUG)
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

  SplashScreen::Show();
  SplashScreen::SetMessage("Initializing logger...");

  // Initialize logging system (overwrites log file each launch)
  Logger::Instance();

  SplashScreen::SetMessage("Running library bootstrap...");
  ProjectUtils::RunStartupLibraryBootstrap();

  SplashScreen::SetMessage("Creating main window...");
  MainWindow *mainWindow = new MainWindow(app::kName);
  mainWindow->Show(true);
  // Start maximized so minimize and restore buttons remain available
  mainWindow->Maximize(true);

  std::optional<std::string> startupPathOpt = GetStartupPathFromArgs(
      argc, argv, launchWorkingDirectoryUtf8);
  if (!startupPathOpt)
    startupPathOpt = ConsumePendingExternalOpenPath();

  SplashScreen::SetMessage("Loading last project...");
  wxWeakRef<MainWindow> mainWindowRef(mainWindow);
  auto lastPathOpt = ProjectUtils::LoadLastProjectPath();

  if (startupPathOpt && mainWindowRef) {
    // Route startup file opens through EVT_PROJECT_LOADED so startup-reset and
    // command-line open cannot race each other. The actual open/import is
    // deferred by MainWindow::OnProjectLoaded() after startup pending state is
    // cleared.
    QueueProjectLoadedEvent(mainWindowRef, false, false, *startupPathOpt);
  } else if (lastPathOpt) {
    std::string lastPath = *lastPathOpt;
    mainWindow->CallAfter([this, mainWindowRef, lastPath]() {
      if (!mainWindowRef)
        return;
      if (auto pendingOpenPath = ConsumePendingExternalOpenPath()) {
        QueueProjectLoadedEvent(mainWindowRef, false, false, *pendingOpenPath);
        return;
      }

      // Give macOS open-document events one additional UI tick to arrive
      // before committing to loading the last project path by default.
      mainWindowRef->CallAfter([this, mainWindowRef, lastPath]() {
        if (!mainWindowRef)
          return;
        if (auto pendingOpenPath = ConsumePendingExternalOpenPath()) {
          QueueProjectLoadedEvent(mainWindowRef, false, false, *pendingOpenPath);
          return;
        }
        mainWindowRef->LoadStartupProjectFromPath(lastPath);
      });
    });

  } else if (mainWindowRef) {
    QueueProjectLoadedEvent(mainWindowRef, false, false);
  }

  return true;
}

// Routes a single macOS file-open request to the shared external-open handler.
void MyApp::MacOpenFile(const wxString &fileName) {
  const std::string rawPathUtf8 = WxStringToDeterministicUtf8Path(fileName);
  Logger::Instance().Log("MacOpenFile received: " + rawPathUtf8);
  HandleExternalOpenPath(NormalizeExternalOpenPath(rawPathUtf8));
}

// Routes macOS multi-file open requests to the external open pipeline in order.
void MyApp::MacOpenFiles(const wxArrayString &fileNames) {
  Logger::Instance().Log("MacOpenFiles received count: " +
                         std::to_string(fileNames.GetCount()));
  for (const wxString &fileName : fileNames) {
    MacOpenFile(fileName);
  }
}


// Routes macOS URL open requests (Finder/LaunchServices) to the shared external-open handler.
void MyApp::MacOpenURL(const wxString &url) {
  const std::string rawUrlUtf8 = WxStringToDeterministicUtf8Path(url);
  Logger::Instance().Log("MacOpenURL received: " + rawUrlUtf8);
  HandleExternalOpenPath(NormalizeExternalOpenPath(rawUrlUtf8));
}

// Handles external file-open requests by deferring them until startup loading is safe.
void MyApp::HandleExternalOpenPath(const std::string &pathUtf8) {
  if (pathUtf8.empty())
    return;

  MainWindow *mainWindow = wxDynamicCast(GetTopWindow(), MainWindow);
  if (!mainWindow) {
    Logger::Instance().Log(
        "HandleExternalOpenPath queued: MainWindow is not ready.");
    pending_external_open_paths_.push_back(pathUtf8);
    return;
  }

  bool startupEventAlreadySent = project_load_event_sent_.load();
  if (!startupEventAlreadySent) {
    Logger::Instance().Log(
        "HandleExternalOpenPath routing through startup project-loaded pipeline.");
    QueueProjectLoadedEvent(wxWeakRef<MainWindow>(mainWindow), false, true, pathUtf8);
    return;
  }

  Logger::Instance().Log(
      "HandleExternalOpenPath routing through MainWindow deferred-open pipeline.");
  mainWindow->CallAfter([windowRef = wxWeakRef<MainWindow>(mainWindow),
                         pathUtf8]() {
    if (!windowRef)
      return;
    windowRef->EnqueueExternalOpenPath(pathUtf8);
  });
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
  last_event_summary_ = wxString::Format(
                            "Last event: class=%s type=%d id=%d object=%s",
                            eventClassName, static_cast<int>(event.GetEventType()),
                            event.GetId(), objectClassName)
                            .ToStdString();
  return -1;
}

namespace {
// Logs an exception message and an optional platform stack trace for diagnostics.
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

// Logs recoverable main-loop exceptions and keeps the app running when possible.
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

// Logs non-recoverable unhandled exceptions for post-mortem diagnostics.
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
