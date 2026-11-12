/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */
#include "positiontoolpanel.h"
#include "configmanager.h"
#include "matrixutils.h"
#include "scene_node_operations.h"

#include <cassert>
#include <wx/app.h>
#include <wx/button.h>
#include <wx/choicdlg.h>
#include <wx/modalhook.h>
#include <wx/frame.h>
#include <wx/sizer.h>
#include <wx/spinbutt.h>
#include <wx/textctrl.h>

class PositionTestApp final : public wxApp {
public:
  bool OnInit() override { return true; }
};
wxIMPLEMENT_APP_NO_MAIN(PositionTestApp);

namespace {
wxTextCtrl *Field(wxWindow &parent, const wxString &name) {
  for (auto *child : parent.GetChildren()) {
    if (auto *field = dynamic_cast<wxTextCtrl *>(child); field && field->GetName() == name)
      return field;
    if (auto *found = Field(*child, name)) return found;
  }
  return nullptr;
}
class GroupChoice final : public wxModalDialogHook {
public:
  int Enter(wxDialog *dialog) override {
    if (auto *choice = dynamic_cast<wxSingleChoiceDialog *>(dialog)) {
      choice->SetSelection(0);
      return wxID_OK;
    }
    return wxID_NONE;
  }
};
void Flush() { wxTheApp->ProcessPendingEvents(); wxYield(); }
void Enter(wxTextCtrl &field) {
  wxCommandEvent event(wxEVT_TEXT_ENTER, field.GetId());
  field.GetEventHandler()->ProcessEvent(event);
}
}

int main(int argc, char **argv) {
  assert(wxEntryStart(argc, argv));
  assert(wxTheApp->CallOnInit());
  auto &config = ConfigManager::Get();
  config.Reset();
  auto *frame = new wxFrame(nullptr, wxID_ANY, "Position test");
  int publications = 0;
  auto *panel = new PositionToolPanel(frame, config,
      [&](const scene_grouping::ObjectSelection &selection, bool sceneChanged) {
        if (sceneChanged) ++publications;
        config.SetSelectedFixtures(selection.fixtures);
        config.SetSelectedTrusses(selection.trusses);
        config.SetSelectedSupports(selection.supports);
        config.SetSelectedSceneObjects(selection.sceneObjects);
        config.SetSelectedGroupObjects(selection.groupObjects);
      });
  auto *sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(panel, 1, wxEXPAND);
  frame->SetSizer(sizer);
  frame->SetClientSize(420, 140);
  frame->Show();
  Flush();
  auto *x = Field(*panel, "X:");
  auto *y = Field(*panel, "Y:");
  auto *z = Field(*panel, "Z:");
  assert(x && y && z && !x->IsEnabled());
  Fixture a;
  a.uuid = "fixture-a";
  a.transform = MatrixUtils::Identity();
  a.transform.o = {1234.125f, 2000, 3000};
  config.GetScene().fixtures[a.uuid] = a;
  Fixture b = a;
  b.uuid = "fixture-b";
  b.transform.o[0] = -4000;
  config.GetScene().fixtures[b.uuid] = b;
  config.SetSelectedFixtures({a.uuid});
  Flush();
  assert(x->IsEnabled() && x->GetValue() == "1.234");
  x->SetFocus(); Flush();
  // Unedited focus loss/Enter must not round the full stored origin.
  Enter(*x);
  assert(config.GetScene().fixtures.at(a.uuid).transform.o[0] == 1234.125f);
  x->SetValue("5"); Enter(*x); Flush();
  assert(x->HasFocus() && publications == 1);
  assert(config.GetScene().fixtures.at(a.uuid).transform.o[0] == 5000);
  assert(config.GetScene().fixtures.at(a.uuid).transform.o[1] == 2000);
  Enter(*x); Flush();
  assert(publications == 1);
  assert(config.Undo() == "Position Tool" && !config.CanUndo());
  Flush();
  assert(x->GetValue() == "1.234");
  assert(config.Redo() == "Position Tool"); Flush();
  assert(x->GetValue() == "5.000");
  x->SetValue("8");
  wxKeyEvent escape(wxEVT_CHAR_HOOK);
  escape.m_keyCode = WXK_ESCAPE;
  x->GetEventHandler()->ProcessEvent(escape);
  Enter(*x);
  assert(publications == 1 && x->GetValue() == "5.000");
  x->SetValue("invalid"); Enter(*x);
  assert(publications == 1);
  // A scene refresh cannot overwrite a pending text edit.
  config.NotifyChange(ConfigManager::Change::Scene); Flush();
  assert(x->GetValue() == "invalid");
  x->SetValue("9");
  config.SetSelectedFixtures({b.uuid});
  // Even before the deferred refresh runs, the pending edit is selection-bound.
  Enter(*x); Flush();
  assert(config.GetScene().fixtures.at(b.uuid).transform.o[0] == -4000);
  assert(x->GetValue() == "-4.000");
  config.SetSelectedFixtures({a.uuid, b.uuid}); Flush();
  assert(x->GetValue().empty() && y->GetValue() == "2.000");
  x->SetValue(" "); Enter(*x); Flush();
  assert(publications == 1 && x->GetValue().empty());
  x->SetValue("-2"); Enter(*x); Flush();
  assert(publications == 2);
  assert(config.GetScene().fixtures.at(a.uuid).transform.o[0] == -2000);
  assert(config.GetScene().fixtures.at(b.uuid).transform.o[0] == -2000);
  assert(config.Undo() == "Position Tool"); Flush();
  assert(x->GetValue().empty());
  assert(config.Redo() == "Position Tool"); Flush();
  assert(x->GetValue() == "-2.000");
  x->SetFocus(); Flush(); x->SetValue("invalid"); y->SetFocus(); Flush();
  assert(publications == 2 && x->GetValue() == "-2.000");
  // Focus change commits exactly once.
  y->SetFocus(); Flush(); y->SetValue("4"); z->SetFocus(); Flush();
  assert(publications == 3 && y->GetValue() == "4.000");
  config.SetValue("ui_distance_unit_system", "imperial"); Flush();
  assert(x->GetValue() == "-6.562");
  config.SetValue("ui_distance_unit_system", "metric"); Flush();
  auto matrix = config.GetScene().fixtures.at(a.uuid).transform;
  matrix.o[2] = 8500;
  assert(scene_node_operations::ApplyExactWorldTransform(config.GetScene(),
      MvrNodeType::Fixture, a.uuid, matrix));
  config.NotifyChange(ConfigManager::Change::Scene); Flush();
  assert(z->GetValue().empty());
  config.SetSelectedFixtures({a.uuid}); Flush();
  assert(z->GetValue() == "8.500");
  frame->SetClientSize(180, 240); Flush();
  const auto topLeft = x->GetParent()->GetPosition();
  assert(topLeft.x >= 0 && x->GetParent()->GetSize().x <= panel->GetSize().x);
  const auto group = scene_grouping::GroupSelection(config.GetScene(), {{a.uuid, b.uuid}});
  assert(group.changed);
  config.NotifyChange(ConfigManager::Change::Scene); Flush();
  for (auto *child : panel->GetChildren())
    assert(!dynamic_cast<wxButton *>(child));
  GroupChoice choose;
  choose.Register();
  panel->SelectGroup();
  choose.Unregister();
  Flush();
  assert(config.GetSelectedGroupObjects() == std::vector<std::string>{group.groupUuid});
  assert(z->GetValue() == "5.750");
  const Matrix childLocal = config.GetScene().fixtures.at(a.uuid).localTransform;
  z->SetValue("9"); Enter(*z); Flush();
  assert(publications == 4 && z->GetValue() == "9.000");
  assert(config.GetScene().fixtures.at(a.uuid).localTransform.o == childLocal.o);
  assert(config.Undo() == "Position Tool"); Flush();
  assert(z->GetValue() == "5.750");
  assert(config.Redo() == "Position Tool"); Flush();
  assert(z->GetValue() == "9.000");
  wxSpinButton *stepper = nullptr;
  for (auto *child : z->GetParent()->GetChildren())
    if (auto *spin = dynamic_cast<wxSpinButton *>(child)) stepper = spin;
  assert(stepper && stepper->IsEnabled());
  wxSpinEvent step(wxEVT_SPIN_UP, stepper->GetId());
  stepper->GetEventHandler()->ProcessEvent(step); Flush();
  assert(publications == 5 && z->GetValue() == "9.001");
  assert(config.Undo() == "Position Tool"); Flush();
  assert(z->GetValue() == "9.000");
  config.SetSelectedFixtures({}); Flush();
  assert(!x->IsEnabled() && x->GetValue().empty() && !stepper->IsEnabled());
  frame->Destroy(); Flush();
  // Destroying the panel unregisters its callback before later publications.
  config.NotifyChange(ConfigManager::Change::Scene); Flush();
  wxTheApp->OnExit();
  wxEntryCleanup();
}
