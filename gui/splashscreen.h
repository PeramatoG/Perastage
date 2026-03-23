#pragma once
#include <wx/string.h>

class SplashScreen {
public:
  static void Show();
  static void SetMessage(const wxString &message);
  static void Hide();
};
