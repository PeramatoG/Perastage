#pragma once

#include <wx/panel.h>
#include <string>

class FixturePreviewPanel;
class wxChoice;

// One scalable selector for assembled preview or an individual Geometry subtree.
class FixturePartNavigator : public wxPanel {
public:
  explicit FixturePartNavigator(wxWindow *parent);
  void LoadFixture(const std::string &path);
private:
  wxChoice *selector_ = nullptr;
  FixturePreviewPanel *preview_ = nullptr;
};
