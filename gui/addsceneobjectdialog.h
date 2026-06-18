#pragma once

#include <wx/dialog.h>

class wxCheckBox;
class wxCommandEvent;
class wxSpinCtrl;

struct AddSceneObjectRequest {
  int quantity = 1;
  bool continuousPlacement = false;
};

class AddSceneObjectDialog final : public wxDialog {
public:
  explicit AddSceneObjectDialog(wxWindow *parent);

  AddSceneObjectRequest GetRequest() const;

private:
  void OnContinuousPlacementChanged(wxCommandEvent &event);

  wxSpinCtrl *quantityCtrl_ = nullptr;
  wxCheckBox *continuousPlacementCtrl_ = nullptr;
};
