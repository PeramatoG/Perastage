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
#include <thread>
#include <wx/sysopt.h>
#include <wx/weakref.h>
#include <wx/wx.h>

class MyApp : public wxApp {
public:
  virtual bool OnInit() override;
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
