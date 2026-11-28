// This file is part of Perastage. License: GPL-3.0-or-later.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <wx/colour.h>
#include <wx/scrolwin.h>

class wxStaticText;

namespace gui::properties {
struct Choice {
  std::vector<wxString> options;
  int selected;
  bool operator==(const Choice &) const = default;
};
struct Font {
  std::string reference;
  bool bold, italic;
  bool operator==(const Font &) const = default;
};
struct Action {
  bool operator==(const Action &) const = default;
};
using Value = std::variant<wxString, double, bool, Choice, wxColour, Font, Action>;
struct Row {
  int id;
  wxString label;
  Value value;
  bool editable = false;
  bool multiline = false;
  bool operator==(const Row &) const = default;
};
struct Section {
  wxString label;
  std::vector<Row> rows;
  bool operator==(const Section &) const = default;
};
struct Sheet {
  std::size_t generation = 0;
  wxString title;
  std::vector<Section> sections;
  bool operator==(const Sheet &) const = default;
};

class Panel final : public wxScrolledWindow {
public:
  using Read = std::function<Sheet()>;
  using Commit = std::function<bool(int, const Value &)>;
  Panel(wxWindow *parent, Read read, Commit commit);
  void RequestRefresh();

private:
  void Rebuild();
  void AddRow(wxSizer &section, const Row &row);
  void Submit(int id, const Value &value);

  Read read_;
  Commit commit_;
  Sheet sheet_;
  wxPanel *body_ = nullptr;
  wxStaticText *status_ = nullptr;
  bool pending_ = false;
  bool rejected_ = false;
  bool committing_ = false;
};
} // namespace gui::properties
