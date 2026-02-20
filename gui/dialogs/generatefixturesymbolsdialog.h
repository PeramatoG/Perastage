#pragma once

#include <string>
#include <vector>

#include <wx/dialog.h>

class wxListBox;

struct FixtureTypeOption {
  std::string typeName;
  std::string gdtfSpec;
  std::string modelKey;
  std::string representativeFixtureUuid;
  int instanceCount = 0;
};

class GenerateFixtureSymbolsDialog : public wxDialog {
public:
  GenerateFixtureSymbolsDialog(wxWindow *parent,
                               const std::vector<FixtureTypeOption> &options);

  int GetSelectedIndex() const;

private:
  void OnGenerate(wxCommandEvent &event);

  wxListBox *listBox = nullptr;
};
