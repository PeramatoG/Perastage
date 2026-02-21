#pragma once

#include <string>
#include <vector>

#include <wx/dialog.h>

struct FixtureSymbolTypeOption {
  std::string label;
  std::vector<std::string> modelKeys;
};

class wxListBox;

class GenerateFixtureSymbolsDialog : public wxDialog {
public:
  GenerateFixtureSymbolsDialog(wxWindow *parent,
                               const std::vector<FixtureSymbolTypeOption> &options);

  int GetSelectionIndex() const;

private:
  wxListBox *list_ = nullptr;
};
