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
#include "configmanager.h"
#include "logger.h"
#include "mainwindow.h"
#include "projectutils.h"
#include "splashscreen.h"
#include <filesystem>
#include <new>
#include <thread>
#include <wx/stackwalk.h>
#include <wx/sysopt.h>
#include <wx/weakref.h>
#include <wx/wx.h>

class MyApp : public wxApp {
public:
  virtual bool OnInit() override;
  int FilterEvent(wxEvent &event) override;
  bool OnExceptionInMainLoop() override;
  void OnUnhandledException() override;

private:
  std::string last_event_summary_;
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit() {
  SetAppName("Perastage");
  SetVendorName("Perasoft");

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
  MainWindow *mainWindow = new MainWindow("Perastage");
  mainWindow->Show(true);
  // Start maximized so minimize and restore buttons remain available
  mainWindow->Maximize(true);

  SplashScreen::SetMessage("Loading last project...");
  wxWeakRef<MainWindow> mainWindowRef(mainWindow);
  auto lastPathOpt = ProjectUtils::LoadLastProjectPath();

  if (lastPathOpt) {
    std::string lastPath = *lastPathOpt;
    std::thread([mainWindowRef, lastPath]() {
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
        if (mainWindowRef) {
          wxCommandEvent evt(EVT_PROJECT_LOADED);
          evt.SetInt(loaded ? 1 : 0);
          evt.SetExtraLong(clearLastProject ? 1 : 0);
          evt.SetString(path);
          wxQueueEvent(mainWindowRef.get(), evt.Clone());
        }
      } catch (const std::exception &ex) {
        Logger::Instance().Log(
            std::string("Failed to load last project: ") + ex.what());
        if (mainWindowRef) {
          wxCommandEvent evt(EVT_PROJECT_LOADED);
          evt.SetInt(0);
          evt.SetExtraLong(1);
          wxQueueEvent(mainWindowRef.get(), evt.Clone());
        }
      } catch (...) {
        Logger::Instance().Log("Failed to load last project: unknown error.");
        if (mainWindowRef) {
          wxCommandEvent evt(EVT_PROJECT_LOADED);
          evt.SetInt(0);
          evt.SetExtraLong(1);
          wxQueueEvent(mainWindowRef.get(), evt.Clone());
        }
      }
    }).detach();
  } else if (mainWindowRef) {
    wxCommandEvent evt(EVT_PROJECT_LOADED);
    evt.SetInt(0);
    evt.SetExtraLong(0);
    wxQueueEvent(mainWindowRef.get(), evt.Clone());
  }

  return true;
}

int MyApp::FilterEvent(wxEvent &event) {
  const wxClassInfo *eventInfo = event.GetClassInfo();
  wxString eventClassName =
      eventInfo ? eventInfo->GetClassName() : "UnknownEvent";
  wxString objectClassName = "None";
  if (event.GetEventObject()) {
    const wxClassInfo *objectInfo = event.GetEventObject()->GetClassInfo();
    if (objectInfo) {
      objectClassName = objectInfo->GetClassName();
    } else {
      objectClassName = "UnknownObject";
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
