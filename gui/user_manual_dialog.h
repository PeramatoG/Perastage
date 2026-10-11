#pragma once

#include "user_manual/manual_resolver.h"
#include <functional>
#include <vector>
#include <wx/dialog.h>

class wxButton;
class ManualHtmlWindow;
class wxStaticText;

// Presentation and history only; Core owns locale, file IO and link policy.
class UserManualDialog final : public wxDialog {
public:
  using OpenBrowser = std::function<void(const std::string &)>;
  UserManualDialog(wxWindow *parent, const std::filesystem::path &manualRoot,
                   const std::string &uiLanguage, OpenBrowser browser = {});
  bool NavigateLink(const std::string &href);
  bool GoHome();
  bool GoBack();
  const user_manual::Page &CurrentPage() const { return current_; }

private:
  bool OpenPage(const std::string &page, const std::string &fragment,
                bool remember);
  user_manual::ManualResolver resolver_;
  user_manual::Page current_;
  std::vector<user_manual::Page> history_;
  OpenBrowser browser_;
  wxButton *back_ = nullptr;
  ManualHtmlWindow *html_ = nullptr;
  wxStaticText *error_ = nullptr;
};
