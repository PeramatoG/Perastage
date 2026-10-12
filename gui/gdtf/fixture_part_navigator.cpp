#include "gdtf/fixture_part_navigator.h"

#include "fixturepreviewpanel.h"
#include <wx/choice.h>
#include <wx/sizer.h>

FixturePartNavigator::FixturePartNavigator(wxWindow *parent) : wxPanel(parent) {
  auto *sizer = new wxBoxSizer(wxVERTICAL);
  selector_ = new wxChoice(this, wxID_ANY);
  selector_->Append(_("All fixture"));
  selector_->SetSelection(0);
  selector_->SetToolTip(_("Select a Geometry to inspect its subtree in Solo mode."));
  preview_ = new FixturePreviewPanel(this);
  sizer->Add(selector_, 0, wxEXPAND | wxBOTTOM, 3);
  sizer->Add(preview_, 1, wxEXPAND);
  SetSizer(sizer);
  selector_->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) {
    preview_->ShowGeometry(selector_->GetSelection() - 1);
  });
}

void FixturePartNavigator::LoadFixture(const std::string &path) {
  preview_->LoadFixture(path);
  selector_->Clear();
  selector_->Append(_("All fixture"));
  const auto &tree = preview_->GetGeometryTree();
  for (size_t index = 0; index < tree.nodes.size(); ++index) {
    const auto &node = tree.nodes[index];
    std::string hierarchy;
    int ancestor = node.parentIndex;
    for (size_t depth = 0; ancestor >= 0 && ancestor < static_cast<int>(index) &&
         depth < tree.nodes.size(); ++depth) {
      hierarchy += "  ";
      ancestor = tree.nodes[ancestor].parentIndex;
    }
    selector_->Append(wxString::Format(_("%zu: %s [%s: %s]"), index + 1,
        wxString::FromUTF8(hierarchy + (node.geometryName.empty() ? node.stableName : node.geometryName)),
        wxString::FromUTF8(node.modelName), wxString::FromUTF8(node.modelFile)));
  }
  selector_->SetSelection(0);
}
