#pragma once
#include <wx/string.h>

class SplashScreen {
public:
  struct Options {
    bool showLogo = true;
  };

  static void Show();
  static void Show(const Options &options);
  static void SetMessage(const wxString &message);
  static void Hide();
};
