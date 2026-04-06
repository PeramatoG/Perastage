#include "scene_object_primitive_dialogs.h"

#include <algorithm>

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

namespace scene_object_primitives {
namespace {

class SphereDialog : public wxDialog {
public:
  SphereDialog(wxWindow *parent, const wxString &title,
               const SphereRequest &initialRequest, bool includeQuantity)
      : wxDialog(parent, wxID_ANY, title, wxDefaultPosition,
                 wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
        includeQuantity_(includeQuantity) {
    auto *root = new wxBoxSizer(wxVERTICAL);
    auto *grid = new wxFlexGridSizer(2, 2, 8, 8);

    grid->Add(new wxStaticText(this, wxID_ANY, "Radius (m):"),
              0, wxALIGN_CENTER_VERTICAL);
    radiusCtrl_ = new wxSpinCtrlDouble(this, wxID_ANY);
    radiusCtrl_->SetRange(0.01, 1000.0);
    radiusCtrl_->SetIncrement(0.1);
    radiusCtrl_->SetDigits(2);
    radiusCtrl_->SetValue(initialRequest.radiusMeters);
    grid->Add(radiusCtrl_, 1, wxEXPAND);

    if (includeQuantity_) {
      grid->Add(new wxStaticText(this, wxID_ANY, "Units:"),
                0, wxALIGN_CENTER_VERTICAL);
      quantityCtrl_ = new wxSpinCtrl(this, wxID_ANY);
      quantityCtrl_->SetRange(1, 1000);
      quantityCtrl_->SetValue(initialRequest.quantity);
      grid->Add(quantityCtrl_, 1, wxEXPAND);
    }

    grid->AddGrowableCol(1, 1);
    root->Add(grid, 1, wxALL | wxEXPAND, 12);
    root->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL),
              0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 12);

    SetSizerAndFit(root);
  }

  SphereRequest Request() const {
    SphereRequest request;
    request.radiusMeters = radiusCtrl_->GetValue();
    request.quantity = includeQuantity_ ? quantityCtrl_->GetValue() : 1;
    return request;
  }

private:
  wxSpinCtrlDouble *radiusCtrl_ = nullptr;
  wxSpinCtrl *quantityCtrl_ = nullptr;
  bool includeQuantity_ = true;
};

class CubeDialog : public wxDialog {
public:
  CubeDialog(wxWindow *parent, const wxString &title,
             const CubeRequest &initialRequest, bool includeQuantity)
      : wxDialog(parent, wxID_ANY, title, wxDefaultPosition,
                 wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
        includeQuantity_(includeQuantity) {
    auto *root = new wxBoxSizer(wxVERTICAL);
    auto *grid = new wxFlexGridSizer(4, 2, 8, 8);

    lengthCtrl_ = AddDimensionRow(grid, "Length (m):", initialRequest.lengthMeters);
    heightCtrl_ = AddDimensionRow(grid, "Height (m):", initialRequest.heightMeters);
    widthCtrl_ = AddDimensionRow(grid, "Width (m):", initialRequest.widthMeters);

    if (includeQuantity_) {
      grid->Add(new wxStaticText(this, wxID_ANY, "Units:"),
                0, wxALIGN_CENTER_VERTICAL);
      quantityCtrl_ = new wxSpinCtrl(this, wxID_ANY);
      quantityCtrl_->SetRange(1, 1000);
      quantityCtrl_->SetValue(initialRequest.quantity);
      grid->Add(quantityCtrl_, 1, wxEXPAND);
    }

    grid->AddGrowableCol(1, 1);
    root->Add(grid, 1, wxALL | wxEXPAND, 12);
    root->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL),
              0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 12);

    SetSizerAndFit(root);
  }

  CubeRequest Request() const {
    CubeRequest request;
    request.lengthMeters = lengthCtrl_->GetValue();
    request.heightMeters = heightCtrl_->GetValue();
    request.widthMeters = widthCtrl_->GetValue();
    request.quantity = includeQuantity_ ? quantityCtrl_->GetValue() : 1;
    return request;
  }

private:
  wxSpinCtrlDouble *AddDimensionRow(wxFlexGridSizer *grid, const char *label,
                                    double defaultValue) {
    grid->Add(new wxStaticText(this, wxID_ANY, label),
              0, wxALIGN_CENTER_VERTICAL);
    auto *ctrl = new wxSpinCtrlDouble(this, wxID_ANY);
    ctrl->SetRange(0.01, 1000.0);
    ctrl->SetIncrement(0.1);
    ctrl->SetDigits(2);
    ctrl->SetValue(defaultValue);
    grid->Add(ctrl, 1, wxEXPAND);
    return ctrl;
  }

  wxSpinCtrlDouble *lengthCtrl_ = nullptr;
  wxSpinCtrlDouble *heightCtrl_ = nullptr;
  wxSpinCtrlDouble *widthCtrl_ = nullptr;
  wxSpinCtrl *quantityCtrl_ = nullptr;
  bool includeQuantity_ = true;
};

class ScreenEditDialog : public wxDialog {
public:
  ScreenEditDialog(wxWindow *parent, const ScreenEditRequest &initial)
      : wxDialog(parent, wxID_ANY, "Edit Screen", wxDefaultPosition,
                 wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    auto *root = new wxBoxSizer(wxVERTICAL);
    auto *grid = new wxFlexGridSizer(2, 2, 8, 8);

    widthCtrl_ = AddDimensionRow(grid, "Width (m):", initial.widthMeters);
    heightCtrl_ = AddDimensionRow(grid, "Height (m):", initial.heightMeters);

    grid->AddGrowableCol(1, 1);
    root->Add(grid, 1, wxALL | wxEXPAND, 12);
    root->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL),
              0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 12);
    SetSizerAndFit(root);
  }

  ScreenEditRequest Request() const {
    ScreenEditRequest request;
    request.widthMeters = widthCtrl_->GetValue();
    request.heightMeters = heightCtrl_->GetValue();
    return request;
  }

private:
  wxSpinCtrlDouble *AddDimensionRow(wxFlexGridSizer *grid, const char *label,
                                    double defaultValue) {
    grid->Add(new wxStaticText(this, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
    auto *ctrl = new wxSpinCtrlDouble(this, wxID_ANY);
    ctrl->SetRange(0.01, 1000.0);
    ctrl->SetIncrement(0.1);
    ctrl->SetDigits(2);
    ctrl->SetValue(defaultValue);
    grid->Add(ctrl, 1, wxEXPAND);
    return ctrl;
  }

  wxSpinCtrlDouble *widthCtrl_ = nullptr;
  wxSpinCtrlDouble *heightCtrl_ = nullptr;
};

class PipeEditDialog : public wxDialog {
public:
  PipeEditDialog(wxWindow *parent, const PipeEditRequest &initial)
      : wxDialog(parent, wxID_ANY, "Edit Pipe", wxDefaultPosition,
                 wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    auto *root = new wxBoxSizer(wxVERTICAL);
    auto *grid = new wxFlexGridSizer(1, 2, 8, 8);

    grid->Add(new wxStaticText(this, wxID_ANY, "Length (m):"),
              0, wxALIGN_CENTER_VERTICAL);
    lengthCtrl_ = new wxSpinCtrlDouble(this, wxID_ANY);
    lengthCtrl_->SetRange(0.01, 1000.0);
    lengthCtrl_->SetIncrement(0.1);
    lengthCtrl_->SetDigits(2);
    lengthCtrl_->SetValue(initial.lengthMeters);
    grid->Add(lengthCtrl_, 1, wxEXPAND);

    grid->AddGrowableCol(1, 1);
    root->Add(grid, 1, wxALL | wxEXPAND, 12);
    root->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL),
              0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 12);
    SetSizerAndFit(root);
  }

  PipeEditRequest Request() const {
    PipeEditRequest request;
    request.lengthMeters = lengthCtrl_->GetValue();
    return request;
  }

private:
  wxSpinCtrlDouble *lengthCtrl_ = nullptr;
};

constexpr double kMetersToMillimeters = 1000.0;
constexpr double kPrimitiveCubeSizeMillimeters = 1000.0;
constexpr double kPrimitiveSphereDiameterMillimeters = 1000.0;

} // namespace

bool ShowSphereDialog(wxWindow *parent, SphereRequest &outRequest) {
  SphereDialog dialog(parent, "Add Sphere", SphereRequest{}, true);
  if (dialog.ShowModal() != wxID_OK)
    return false;

  outRequest = dialog.Request();
  return true;
}

bool ShowCubeDialog(wxWindow *parent, CubeRequest &outRequest) {
  CubeDialog dialog(parent, "Add Cube", CubeRequest{}, true);
  if (dialog.ShowModal() != wxID_OK)
    return false;

  outRequest = dialog.Request();
  return true;
}

bool ShowSphereEditDialog(wxWindow *parent, SphereRequest &inOutRequest) {
  SphereDialog dialog(parent, "Edit Sphere", inOutRequest, false);
  if (dialog.ShowModal() != wxID_OK)
    return false;

  inOutRequest = dialog.Request();
  return true;
}

bool ShowCubeEditDialog(wxWindow *parent, CubeRequest &inOutRequest) {
  CubeDialog dialog(parent, "Edit Cube", inOutRequest, false);
  if (dialog.ShowModal() != wxID_OK)
    return false;

  inOutRequest = dialog.Request();
  return true;
}

bool ShowScreenEditDialog(wxWindow *parent, ScreenEditRequest &inOutRequest) {
  ScreenEditDialog dialog(parent, inOutRequest);
  if (dialog.ShowModal() != wxID_OK)
    return false;

  inOutRequest = dialog.Request();
  return true;
}

bool ShowPipeEditDialog(wxWindow *parent, PipeEditRequest &inOutRequest) {
  PipeEditDialog dialog(parent, inOutRequest);
  if (dialog.ShowModal() != wxID_OK)
    return false;

  inOutRequest = dialog.Request();
  return true;
}

Matrix BuildSphereScaleTransform(double radiusMeters) {
  const double diameterMm =
      std::max(radiusMeters, 0.01) * 2.0 * kMetersToMillimeters;
  const float uniformScale =
      static_cast<float>(diameterMm / kPrimitiveSphereDiameterMillimeters);

  Matrix transform;
  transform.u = {uniformScale, 0.0f, 0.0f};
  transform.v = {0.0f, uniformScale, 0.0f};
  transform.w = {0.0f, 0.0f, uniformScale};
  return transform;
}

Matrix BuildCubeScaleTransform(double lengthMeters, double heightMeters,
                               double widthMeters) {
  const float sx = static_cast<float>(
      std::max(lengthMeters, 0.01) * kMetersToMillimeters /
      kPrimitiveCubeSizeMillimeters);
  const float sy = static_cast<float>(
      std::max(heightMeters, 0.01) * kMetersToMillimeters /
      kPrimitiveCubeSizeMillimeters);
  const float sz = static_cast<float>(
      std::max(widthMeters, 0.01) * kMetersToMillimeters /
      kPrimitiveCubeSizeMillimeters);

  Matrix transform;
  transform.u = {sx, 0.0f, 0.0f};
  transform.v = {0.0f, sy, 0.0f};
  transform.w = {0.0f, 0.0f, sz};
  return transform;
}

} // namespace scene_object_primitives
