#include "scene_object_primitive_dialogs.h"
#include "configmanager.h"
#include "guiconfigservices.h"
#include "ui_unit_utils.h"
#include "units/units.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace scene_object_primitives {
namespace {

constexpr double kMetersToMillimeters = 1000.0;
constexpr int kSaveDefaultButtonId = wxID_HIGHEST + 211;
constexpr int kLoadDefaultButtonId = wxID_HIGHEST + 212;
constexpr int kApplyPrimitiveButtonId = wxID_HIGHEST + 213;

Units::DistanceUnitSystem ResolveDistanceUnitSystem() {
  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  return UiUnitUtils::ParseDistanceUnitSystem(
      cfg.GetValue("ui_distance_unit_system"));
}

double MetersToDisplayDistance(double meters, Units::DistanceUnitSystem unitSystem) {
  return UiUnitUtils::DistanceMillimetersToDisplay(meters * kMetersToMillimeters,
                                                   unitSystem);
}

// Converts millimeter scene coordinates to the active display distance unit.
double MillimetersToDisplayDistance(double millimeters,
                                    Units::DistanceUnitSystem unitSystem) {
  return UiUnitUtils::DistanceMillimetersToDisplay(millimeters, unitSystem);
}

double DisplayDistanceToMeters(double displayValue,
                               Units::DistanceUnitSystem unitSystem) {
  return UiUnitUtils::DistanceDisplayToMillimeters(displayValue, unitSystem) /
         kMetersToMillimeters;
}

// Converts a displayed distance value to millimeters for scene coordinates.
double DisplayDistanceToMillimeters(double displayValue,
                                    Units::DistanceUnitSystem unitSystem) {
  return UiUnitUtils::DistanceDisplayToMillimeters(displayValue, unitSystem);
}

wxString DistanceLabel(const char *baseLabel,
                       Units::DistanceUnitSystem unitSystem) {
  const std::string suffix = UiUnitUtils::DistanceUnitSuffix(unitSystem);
  return wxString::Format("%s (%s):", baseLabel,
                          suffix.c_str());
}

wxTextCtrl *AddNameRow(wxWindow *parent, wxFlexGridSizer *grid,
                       const std::string &name) {
  grid->Add(new wxStaticText(parent, wxID_ANY, "Name:"), 0,
            wxALIGN_CENTER_VERTICAL);
  auto *ctrl = new wxTextCtrl(parent, wxID_ANY, wxString::FromUTF8(name));
  grid->Add(ctrl, 1, wxEXPAND);
  return ctrl;
}

wxSpinCtrlDouble *AddDistanceRow(wxWindow *parent, wxFlexGridSizer *grid,
                                 const char *label, double meters,
                                 Units::DistanceUnitSystem unitSystem) {
  grid->Add(new wxStaticText(parent, wxID_ANY, DistanceLabel(label, unitSystem)),
            0, wxALIGN_CENTER_VERTICAL);
  auto *ctrl = new wxSpinCtrlDouble(parent, wxID_ANY);
  ctrl->SetRange(MetersToDisplayDistance(0.01, unitSystem),
                 MetersToDisplayDistance(1000.0, unitSystem));
  ctrl->SetIncrement(0.1);
  ctrl->SetDigits(2);
  ctrl->SetValue(MetersToDisplayDistance(meters, unitSystem));
  grid->Add(ctrl, 1, wxEXPAND);
  return ctrl;
}

// Adds a generic numeric spin row to a primitive dialog.
wxSpinCtrlDouble *AddNumberRow(wxWindow *parent, wxFlexGridSizer *grid,
                               const char *label, double value, double minValue,
                               double maxValue, double increment) {
  grid->Add(new wxStaticText(parent, wxID_ANY, label), 0,
            wxALIGN_CENTER_VERTICAL);
  auto *ctrl = new wxSpinCtrlDouble(parent, wxID_ANY);
  ctrl->SetRange(minValue, maxValue);
  ctrl->SetIncrement(increment);
  ctrl->SetDigits(2);
  ctrl->SetValue(value);
  grid->Add(ctrl, 1, wxEXPAND);
  return ctrl;
}

// Adds a signed millimeter distance spin row using the active project unit.
wxSpinCtrlDouble *AddSignedDistanceRow(wxWindow *parent, wxFlexGridSizer *grid,
                                       const char *label, double millimeters,
                                       Units::DistanceUnitSystem unitSystem) {
  grid->Add(new wxStaticText(parent, wxID_ANY, DistanceLabel(label, unitSystem)),
            0, wxALIGN_CENTER_VERTICAL);
  auto *ctrl = new wxSpinCtrlDouble(parent, wxID_ANY);
  ctrl->SetRange(MillimetersToDisplayDistance(-1000000.0, unitSystem),
                 MillimetersToDisplayDistance(1000000.0, unitSystem));
  ctrl->SetIncrement(0.1);
  ctrl->SetDigits(2);
  ctrl->SetValue(MillimetersToDisplayDistance(millimeters, unitSystem));
  grid->Add(ctrl, 1, wxEXPAND);
  return ctrl;
}

wxSizer *CreatePrimitiveButtonSizer(wxWindow *parent, bool includeApply) {
  auto *buttons = new wxBoxSizer(wxHORIZONTAL);
  buttons->Add(new wxButton(parent, kSaveDefaultButtonId, "Save as Default"), 0,
               wxRIGHT, 6);
  buttons->Add(new wxButton(parent, kLoadDefaultButtonId, "Load Default"), 0,
               wxRIGHT, 6);
  buttons->AddStretchSpacer(1);
  if (includeApply)
    buttons->Add(new wxButton(parent, kApplyPrimitiveButtonId, "Apply"), 0,
                 wxRIGHT, 6);
  buttons->Add(new wxButton(parent, wxID_OK, "OK"), 0, wxRIGHT, 6);
  buttons->Add(new wxButton(parent, wxID_CANCEL, "Cancel"), 0);
  return buttons;
}

std::string ReadProjectString(const char *key, const std::string &fallback) {
  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  return cfg.GetValue(key).value_or(fallback);
}

double ReadProjectDistance(const char *key, double fallback) {
  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  auto value = cfg.GetValue(key);
  if (!value)
    return fallback;
  try {
    return std::max(std::stod(*value), 0.01);
  } catch (...) {
    return fallback;
  }
}

void WriteProjectString(const char *key, const std::string &value) {
  GetDefaultGuiConfigServices().LegacyConfigManager().SetValue(key, value);
}

void WriteProjectDistance(const char *key, double value) {
  GetDefaultGuiConfigServices().LegacyConfigManager().SetValue(
      key, std::to_string(std::max(value, 0.01)));
}


class SphereDialog : public wxDialog {
public:
  SphereDialog(wxWindow *parent, const wxString &title,
               const SphereRequest &initialRequest, bool includeQuantity,
               PrimitivePlacementRequest *placement = nullptr,
               SphereRequest *liveRequest = nullptr,
               const PrimitiveApplyCallback &applyCallback = {})
      : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
        includeQuantity_(includeQuantity), placement_(placement),
        liveRequest_(liveRequest), applyCallback_(applyCallback), unitSystem_(ResolveDistanceUnitSystem()) {
    auto *root = new wxBoxSizer(wxVERTICAL);
    auto *grid = new wxFlexGridSizer(includeQuantity ? 3 : 8, 2, 8, 8);
    nameCtrl_ = AddNameRow(this, grid, initialRequest.name);
    radiusCtrl_ = AddDistanceRow(this, grid, "Radius", initialRequest.radiusMeters,
                                 unitSystem_);
    AddQuantityAndPlacementRows(grid, initialRequest.quantity);
    grid->AddGrowableCol(1, 1);
    root->Add(grid, 1, wxALL | wxEXPAND, 12);
    root->Add(CreatePrimitiveButtonSizer(this, !includeQuantity_), 0,
              wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 12);
    Bind(wxEVT_BUTTON, &SphereDialog::OnSaveDefault, this, kSaveDefaultButtonId);
    Bind(wxEVT_BUTTON, &SphereDialog::OnLoadDefault, this, kLoadDefaultButtonId);
    Bind(wxEVT_BUTTON, &SphereDialog::OnApply, this, kApplyPrimitiveButtonId);
    SetSizerAndFit(root);
  }

  // Builds the sphere request from dialog controls.
  SphereRequest Request() const {
    SphereRequest request;
    request.name = nameCtrl_->GetValue().ToStdString();
    if (request.name.empty())
      request.name = "Sphere";
    request.radiusMeters = std::max(
        DisplayDistanceToMeters(radiusCtrl_->GetValue(), unitSystem_), 0.01);
    request.quantity = includeQuantity_ ? quantityCtrl_->GetValue() : 1;
    return request;
  }

  // Builds the placement request from dialog controls.
  PrimitivePlacementRequest Placement() const {
    PrimitivePlacementRequest request;
    if (!placement_)
      return request;
    request.positionXMeters =
        DisplayDistanceToMillimeters(positionXCtrl_->GetValue(), unitSystem_);
    request.positionYMeters =
        DisplayDistanceToMillimeters(positionYCtrl_->GetValue(), unitSystem_);
    request.positionZMeters =
        DisplayDistanceToMillimeters(positionZCtrl_->GetValue(), unitSystem_);
    request.rotationXDegrees = rotationXCtrl_->GetValue();
    request.rotationYDegrees = rotationYCtrl_->GetValue();
    request.rotationZDegrees = rotationZCtrl_->GetValue();
    return request;
  }

private:
  // Adds quantity controls for creation or placement controls for editing.
  void AddQuantityAndPlacementRows(wxFlexGridSizer *grid, int quantity) {
    if (includeQuantity_) {
      grid->Add(new wxStaticText(this, wxID_ANY, "Units:"), 0, wxALIGN_CENTER_VERTICAL);
      quantityCtrl_ = new wxSpinCtrl(this, wxID_ANY);
      quantityCtrl_->SetRange(1, 1000);
      quantityCtrl_->SetValue(quantity);
      grid->Add(quantityCtrl_, 1, wxEXPAND);
      return;
    }
    positionXCtrl_ = AddSignedDistanceRow(
        this, grid, "Position X", placement_->positionXMeters, unitSystem_);
    positionYCtrl_ = AddSignedDistanceRow(
        this, grid, "Position Y", placement_->positionYMeters, unitSystem_);
    positionZCtrl_ = AddSignedDistanceRow(
        this, grid, "Position Z", placement_->positionZMeters, unitSystem_);
    rotationXCtrl_ = AddNumberRow(this, grid, "Rotation X (deg):", placement_->rotationXDegrees, -3600.0, 3600.0, 1.0);
    rotationYCtrl_ = AddNumberRow(this, grid, "Rotation Y (deg):", placement_->rotationYDegrees, -3600.0, 3600.0, 1.0);
    rotationZCtrl_ = AddNumberRow(this, grid, "Rotation Z (deg):", placement_->rotationZDegrees, -3600.0, 3600.0, 1.0);
  }

  void OnSaveDefault(wxCommandEvent &) {
    const SphereRequest request = Request();
    WriteProjectString("primitive_sphere_default_name", request.name);
    WriteProjectDistance("primitive_sphere_default_radius_m", request.radiusMeters);
  }

  void OnLoadDefault(wxCommandEvent &) {
    nameCtrl_->SetValue(wxString::FromUTF8(ReadProjectString("primitive_sphere_default_name", "Sphere")));
    radiusCtrl_->SetValue(MetersToDisplayDistance(ReadProjectDistance("primitive_sphere_default_radius_m", 1.0), unitSystem_));
  }

  void OnApply(wxCommandEvent &) {
    if (liveRequest_)
      *liveRequest_ = Request();
    if (placement_)
      *placement_ = Placement();
    if (applyCallback_)
      applyCallback_();
  }

  wxTextCtrl *nameCtrl_ = nullptr;
  wxSpinCtrlDouble *radiusCtrl_ = nullptr;
  wxSpinCtrl *quantityCtrl_ = nullptr;
  wxSpinCtrlDouble *positionXCtrl_ = nullptr;
  wxSpinCtrlDouble *positionYCtrl_ = nullptr;
  wxSpinCtrlDouble *positionZCtrl_ = nullptr;
  wxSpinCtrlDouble *rotationXCtrl_ = nullptr;
  wxSpinCtrlDouble *rotationYCtrl_ = nullptr;
  wxSpinCtrlDouble *rotationZCtrl_ = nullptr;
  bool includeQuantity_ = true;
  PrimitivePlacementRequest *placement_ = nullptr;
  SphereRequest *liveRequest_ = nullptr;
  PrimitiveApplyCallback applyCallback_;
  Units::DistanceUnitSystem unitSystem_ = Units::DistanceUnitSystem::Metric;
};

class CubeDialog : public wxDialog {
public:
  CubeDialog(wxWindow *parent, const wxString &title,
             const CubeRequest &initialRequest, bool includeQuantity,
             PrimitivePlacementRequest *placement = nullptr,
             CubeRequest *liveRequest = nullptr,
             const PrimitiveApplyCallback &applyCallback = {})
      : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
        includeQuantity_(includeQuantity), placement_(placement),
        liveRequest_(liveRequest), applyCallback_(applyCallback), unitSystem_(ResolveDistanceUnitSystem()) {
    auto *root = new wxBoxSizer(wxVERTICAL);
    auto *grid = new wxFlexGridSizer(includeQuantity ? 5 : 10, 2, 8, 8);
    nameCtrl_ = AddNameRow(this, grid, initialRequest.name);
    lengthCtrl_ = AddDistanceRow(this, grid, "Length", initialRequest.lengthMeters, unitSystem_);
    heightCtrl_ = AddDistanceRow(this, grid, "Height", initialRequest.heightMeters, unitSystem_);
    widthCtrl_ = AddDistanceRow(this, grid, "Width", initialRequest.widthMeters, unitSystem_);
    AddQuantityAndPlacementRows(grid, initialRequest.quantity);
    grid->AddGrowableCol(1, 1);
    root->Add(grid, 1, wxALL | wxEXPAND, 12);
    root->Add(CreatePrimitiveButtonSizer(this, !includeQuantity_), 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 12);
    Bind(wxEVT_BUTTON, &CubeDialog::OnSaveDefault, this, kSaveDefaultButtonId);
    Bind(wxEVT_BUTTON, &CubeDialog::OnLoadDefault, this, kLoadDefaultButtonId);
    Bind(wxEVT_BUTTON, &CubeDialog::OnApply, this, kApplyPrimitiveButtonId);
    SetSizerAndFit(root);
  }

  // Builds the cube request from dialog controls.
  CubeRequest Request() const {
    CubeRequest request;
    request.name = nameCtrl_->GetValue().ToStdString();
    if (request.name.empty())
      request.name = "Cube";
    request.lengthMeters = std::max(
        DisplayDistanceToMeters(lengthCtrl_->GetValue(), unitSystem_), 0.01);
    request.heightMeters = std::max(
        DisplayDistanceToMeters(heightCtrl_->GetValue(), unitSystem_), 0.01);
    request.widthMeters = std::max(
        DisplayDistanceToMeters(widthCtrl_->GetValue(), unitSystem_), 0.01);
    request.quantity = includeQuantity_ ? quantityCtrl_->GetValue() : 1;
    return request;
  }

  // Builds the placement request from dialog controls.
  PrimitivePlacementRequest Placement() const {
    PrimitivePlacementRequest request;
    if (!placement_)
      return request;
    request.positionXMeters =
        DisplayDistanceToMillimeters(positionXCtrl_->GetValue(), unitSystem_);
    request.positionYMeters =
        DisplayDistanceToMillimeters(positionYCtrl_->GetValue(), unitSystem_);
    request.positionZMeters =
        DisplayDistanceToMillimeters(positionZCtrl_->GetValue(), unitSystem_);
    request.rotationXDegrees = rotationXCtrl_->GetValue();
    request.rotationYDegrees = rotationYCtrl_->GetValue();
    request.rotationZDegrees = rotationZCtrl_->GetValue();
    return request;
  }

private:
  // Adds quantity controls for creation or placement controls for editing.
  void AddQuantityAndPlacementRows(wxFlexGridSizer *grid, int quantity) {
    if (includeQuantity_) {
      grid->Add(new wxStaticText(this, wxID_ANY, "Units:"), 0, wxALIGN_CENTER_VERTICAL);
      quantityCtrl_ = new wxSpinCtrl(this, wxID_ANY);
      quantityCtrl_->SetRange(1, 1000);
      quantityCtrl_->SetValue(quantity);
      grid->Add(quantityCtrl_, 1, wxEXPAND);
      return;
    }
    positionXCtrl_ = AddSignedDistanceRow(
        this, grid, "Position X", placement_->positionXMeters, unitSystem_);
    positionYCtrl_ = AddSignedDistanceRow(
        this, grid, "Position Y", placement_->positionYMeters, unitSystem_);
    positionZCtrl_ = AddSignedDistanceRow(
        this, grid, "Position Z", placement_->positionZMeters, unitSystem_);
    rotationXCtrl_ = AddNumberRow(this, grid, "Rotation X (deg):", placement_->rotationXDegrees, -3600.0, 3600.0, 1.0);
    rotationYCtrl_ = AddNumberRow(this, grid, "Rotation Y (deg):", placement_->rotationYDegrees, -3600.0, 3600.0, 1.0);
    rotationZCtrl_ = AddNumberRow(this, grid, "Rotation Z (deg):", placement_->rotationZDegrees, -3600.0, 3600.0, 1.0);
  }

  void OnSaveDefault(wxCommandEvent &) {
    const CubeRequest request = Request();
    WriteProjectString("primitive_cube_default_name", request.name);
    WriteProjectDistance("primitive_cube_default_length_m", request.lengthMeters);
    WriteProjectDistance("primitive_cube_default_height_m", request.heightMeters);
    WriteProjectDistance("primitive_cube_default_width_m", request.widthMeters);
  }

  void OnLoadDefault(wxCommandEvent &) {
    nameCtrl_->SetValue(wxString::FromUTF8(ReadProjectString("primitive_cube_default_name", "Cube")));
    lengthCtrl_->SetValue(MetersToDisplayDistance(ReadProjectDistance("primitive_cube_default_length_m", 1.0), unitSystem_));
    heightCtrl_->SetValue(MetersToDisplayDistance(ReadProjectDistance("primitive_cube_default_height_m", 1.0), unitSystem_));
    widthCtrl_->SetValue(MetersToDisplayDistance(ReadProjectDistance("primitive_cube_default_width_m", 1.0), unitSystem_));
  }

  void OnApply(wxCommandEvent &) {
    if (liveRequest_)
      *liveRequest_ = Request();
    if (placement_)
      *placement_ = Placement();
    if (applyCallback_)
      applyCallback_();
  }

  wxTextCtrl *nameCtrl_ = nullptr;
  wxSpinCtrlDouble *lengthCtrl_ = nullptr;
  wxSpinCtrlDouble *heightCtrl_ = nullptr;
  wxSpinCtrlDouble *widthCtrl_ = nullptr;
  wxSpinCtrl *quantityCtrl_ = nullptr;
  wxSpinCtrlDouble *positionXCtrl_ = nullptr;
  wxSpinCtrlDouble *positionYCtrl_ = nullptr;
  wxSpinCtrlDouble *positionZCtrl_ = nullptr;
  wxSpinCtrlDouble *rotationXCtrl_ = nullptr;
  wxSpinCtrlDouble *rotationYCtrl_ = nullptr;
  wxSpinCtrlDouble *rotationZCtrl_ = nullptr;
  bool includeQuantity_ = true;
  PrimitivePlacementRequest *placement_ = nullptr;
  CubeRequest *liveRequest_ = nullptr;
  PrimitiveApplyCallback applyCallback_;
  Units::DistanceUnitSystem unitSystem_ = Units::DistanceUnitSystem::Metric;
};

class CylinderDialog : public wxDialog {
public:
  CylinderDialog(wxWindow *parent, const wxString &title,
                 const CylinderRequest &initialRequest, bool includeQuantity,
                 PrimitivePlacementRequest *placement = nullptr,
                 CylinderRequest *liveRequest = nullptr,
                 const PrimitiveApplyCallback &applyCallback = {})
      : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
        includeQuantity_(includeQuantity), placement_(placement),
        liveRequest_(liveRequest), applyCallback_(applyCallback), unitSystem_(ResolveDistanceUnitSystem()) {
    auto *root = new wxBoxSizer(wxVERTICAL);
    auto *grid = new wxFlexGridSizer(includeQuantity ? 5 : 10, 2, 8, 8);
    nameCtrl_ = AddNameRow(this, grid, initialRequest.name);
    topRadiusCtrl_ = AddDistanceRow(this, grid, "Top radius", initialRequest.topRadiusMeters, unitSystem_);
    bottomRadiusCtrl_ = AddDistanceRow(this, grid, "Bottom radius", initialRequest.bottomRadiusMeters, unitSystem_);
    heightCtrl_ = AddDistanceRow(this, grid, "Height", initialRequest.heightMeters, unitSystem_);
    AddQuantityAndPlacementRows(grid, initialRequest.quantity);
    grid->AddGrowableCol(1, 1);
    root->Add(grid, 1, wxALL | wxEXPAND, 12);
    root->Add(CreatePrimitiveButtonSizer(this, !includeQuantity_), 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 12);
    Bind(wxEVT_BUTTON, &CylinderDialog::OnSaveDefault, this, kSaveDefaultButtonId);
    Bind(wxEVT_BUTTON, &CylinderDialog::OnLoadDefault, this, kLoadDefaultButtonId);
    Bind(wxEVT_BUTTON, &CylinderDialog::OnApply, this, kApplyPrimitiveButtonId);
    SetSizerAndFit(root);
  }

  // Builds the cylinder request from dialog controls.
  CylinderRequest Request() const {
    CylinderRequest request;
    request.name = nameCtrl_->GetValue().ToStdString();
    if (request.name.empty())
      request.name = "Cylinder";
    request.topRadiusMeters = std::max(
        DisplayDistanceToMeters(topRadiusCtrl_->GetValue(), unitSystem_), 0.01);
    request.bottomRadiusMeters = std::max(
        DisplayDistanceToMeters(bottomRadiusCtrl_->GetValue(), unitSystem_), 0.01);
    request.heightMeters = std::max(
        DisplayDistanceToMeters(heightCtrl_->GetValue(), unitSystem_), 0.01);
    request.quantity = includeQuantity_ ? quantityCtrl_->GetValue() : 1;
    return request;
  }

  // Builds the placement request from dialog controls.
  PrimitivePlacementRequest Placement() const {
    PrimitivePlacementRequest request;
    if (!placement_)
      return request;
    request.positionXMeters =
        DisplayDistanceToMillimeters(positionXCtrl_->GetValue(), unitSystem_);
    request.positionYMeters =
        DisplayDistanceToMillimeters(positionYCtrl_->GetValue(), unitSystem_);
    request.positionZMeters =
        DisplayDistanceToMillimeters(positionZCtrl_->GetValue(), unitSystem_);
    request.rotationXDegrees = rotationXCtrl_->GetValue();
    request.rotationYDegrees = rotationYCtrl_->GetValue();
    request.rotationZDegrees = rotationZCtrl_->GetValue();
    return request;
  }

private:
  // Adds quantity controls for creation or placement controls for editing.
  void AddQuantityAndPlacementRows(wxFlexGridSizer *grid, int quantity) {
    if (includeQuantity_) {
      grid->Add(new wxStaticText(this, wxID_ANY, "Units:"), 0, wxALIGN_CENTER_VERTICAL);
      quantityCtrl_ = new wxSpinCtrl(this, wxID_ANY);
      quantityCtrl_->SetRange(1, 1000);
      quantityCtrl_->SetValue(quantity);
      grid->Add(quantityCtrl_, 1, wxEXPAND);
      return;
    }
    positionXCtrl_ = AddSignedDistanceRow(
        this, grid, "Position X", placement_->positionXMeters, unitSystem_);
    positionYCtrl_ = AddSignedDistanceRow(
        this, grid, "Position Y", placement_->positionYMeters, unitSystem_);
    positionZCtrl_ = AddSignedDistanceRow(
        this, grid, "Position Z", placement_->positionZMeters, unitSystem_);
    rotationXCtrl_ = AddNumberRow(this, grid, "Rotation X (deg):", placement_->rotationXDegrees, -3600.0, 3600.0, 1.0);
    rotationYCtrl_ = AddNumberRow(this, grid, "Rotation Y (deg):", placement_->rotationYDegrees, -3600.0, 3600.0, 1.0);
    rotationZCtrl_ = AddNumberRow(this, grid, "Rotation Z (deg):", placement_->rotationZDegrees, -3600.0, 3600.0, 1.0);
  }

  void OnSaveDefault(wxCommandEvent &) {
    const CylinderRequest request = Request();
    WriteProjectString("primitive_cylinder_default_name", request.name);
    WriteProjectDistance("primitive_cylinder_default_top_radius_m", request.topRadiusMeters);
    WriteProjectDistance("primitive_cylinder_default_bottom_radius_m", request.bottomRadiusMeters);
    WriteProjectDistance("primitive_cylinder_default_height_m", request.heightMeters);
  }

  void OnLoadDefault(wxCommandEvent &) {
    nameCtrl_->SetValue(wxString::FromUTF8(ReadProjectString("primitive_cylinder_default_name", "Cylinder")));
    topRadiusCtrl_->SetValue(MetersToDisplayDistance(ReadProjectDistance("primitive_cylinder_default_top_radius_m", 0.5), unitSystem_));
    bottomRadiusCtrl_->SetValue(MetersToDisplayDistance(ReadProjectDistance("primitive_cylinder_default_bottom_radius_m", 0.5), unitSystem_));
    heightCtrl_->SetValue(MetersToDisplayDistance(ReadProjectDistance("primitive_cylinder_default_height_m", 1.0), unitSystem_));
  }

  void OnApply(wxCommandEvent &) {
    if (liveRequest_)
      *liveRequest_ = Request();
    if (placement_)
      *placement_ = Placement();
    if (applyCallback_)
      applyCallback_();
  }

  wxTextCtrl *nameCtrl_ = nullptr;
  wxSpinCtrlDouble *topRadiusCtrl_ = nullptr;
  wxSpinCtrlDouble *bottomRadiusCtrl_ = nullptr;
  wxSpinCtrlDouble *heightCtrl_ = nullptr;
  wxSpinCtrl *quantityCtrl_ = nullptr;
  wxSpinCtrlDouble *positionXCtrl_ = nullptr;
  wxSpinCtrlDouble *positionYCtrl_ = nullptr;
  wxSpinCtrlDouble *positionZCtrl_ = nullptr;
  wxSpinCtrlDouble *rotationXCtrl_ = nullptr;
  wxSpinCtrlDouble *rotationYCtrl_ = nullptr;
  wxSpinCtrlDouble *rotationZCtrl_ = nullptr;
  bool includeQuantity_ = true;
  PrimitivePlacementRequest *placement_ = nullptr;
  CylinderRequest *liveRequest_ = nullptr;
  PrimitiveApplyCallback applyCallback_;
  Units::DistanceUnitSystem unitSystem_ = Units::DistanceUnitSystem::Metric;
};

class ScreenEditDialog : public wxDialog {
public:
  ScreenEditDialog(wxWindow *parent, const ScreenEditRequest &initial)
      : wxDialog(parent, wxID_ANY, "Edit Screen", wxDefaultPosition,
                 wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
        unitSystem_(ResolveDistanceUnitSystem()) {
    auto *root = new wxBoxSizer(wxVERTICAL);
    auto *grid = new wxFlexGridSizer(2, 2, 8, 8);

    widthCtrl_ = AddDimensionRow(grid, "Width", initial.widthMeters);
    heightCtrl_ = AddDimensionRow(grid, "Height", initial.heightMeters);

    grid->AddGrowableCol(1, 1);
    root->Add(grid, 1, wxALL | wxEXPAND, 12);
    root->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL),
              0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 12);
    SetSizerAndFit(root);
  }

  // Builds the screen edit request from dialog controls.
  ScreenEditRequest Request() const {
    ScreenEditRequest request;
    request.widthMeters =
        DisplayDistanceToMeters(widthCtrl_->GetValue(), unitSystem_);
    request.heightMeters =
        DisplayDistanceToMeters(heightCtrl_->GetValue(), unitSystem_);
    return request;
  }

private:
  // Adds a screen dimension row using the active project distance unit.
  wxSpinCtrlDouble *AddDimensionRow(wxFlexGridSizer *grid, const char *label,
                                    double defaultValue) {
    return AddDistanceRow(this, grid, label, defaultValue, unitSystem_);
  }

  wxSpinCtrlDouble *widthCtrl_ = nullptr;
  wxSpinCtrlDouble *heightCtrl_ = nullptr;
  Units::DistanceUnitSystem unitSystem_ = Units::DistanceUnitSystem::Metric;
};

class PipeEditDialog : public wxDialog {
public:
  PipeEditDialog(wxWindow *parent, const PipeEditRequest &initial)
      : wxDialog(parent, wxID_ANY, "Edit Pipe", wxDefaultPosition,
                 wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
        unitSystem_(ResolveDistanceUnitSystem()) {
    auto *root = new wxBoxSizer(wxVERTICAL);
    auto *grid = new wxFlexGridSizer(1, 2, 8, 8);

    lengthCtrl_ = AddDistanceRow(this, grid, "Length", initial.lengthMeters,
                                 unitSystem_);

    grid->AddGrowableCol(1, 1);
    root->Add(grid, 1, wxALL | wxEXPAND, 12);
    root->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL),
              0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 12);
    SetSizerAndFit(root);
  }

  // Builds the pipe edit request from dialog controls.
  PipeEditRequest Request() const {
    PipeEditRequest request;
    request.lengthMeters =
        DisplayDistanceToMeters(lengthCtrl_->GetValue(), unitSystem_);
    return request;
  }

private:
  wxSpinCtrlDouble *lengthCtrl_ = nullptr;
  Units::DistanceUnitSystem unitSystem_ = Units::DistanceUnitSystem::Metric;
};

constexpr double kPrimitiveCubeSizeMillimeters = 1000.0;
constexpr double kPrimitiveSphereDiameterMillimeters = 1000.0;
constexpr double kPrimitiveCylinderDiameterMillimeters = 1000.0;
constexpr double kPrimitiveCylinderHeightMillimeters = 1000.0;

double ClampDimensionMeters(double value) { return std::max(value, 0.01); }
double AxisLength(const std::array<float, 3> &axis) {
  return std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
}

double ParseFloatTokenValue(std::string_view token, std::string_view key,
                            double fallback) {
  const std::string search = std::string(key) + "=";
  const size_t startPos = token.find(search);
  if (startPos == std::string_view::npos)
    return fallback;
  const size_t valueStart = startPos + search.size();
  size_t valueEnd = token.find_first_of(";,&", valueStart);
  if (valueEnd == std::string_view::npos)
    valueEnd = token.size();

  const std::string value(token.substr(valueStart, valueEnd - valueStart));
  if (value.empty())
    return fallback;
  try {
    return std::stod(value);
  } catch (...) {
    return fallback;
  }
}

} // namespace

bool ShowSphereDialog(wxWindow *parent, SphereRequest &outRequest) {
  SphereRequest defaults;
  defaults.name = ReadProjectString("primitive_sphere_default_name", "Sphere");
  defaults.radiusMeters = ReadProjectDistance("primitive_sphere_default_radius_m", 1.0);
  SphereDialog dialog(parent, "Add Sphere", defaults, true);
  if (dialog.ShowModal() != wxID_OK)
    return false;

  outRequest = dialog.Request();
  return true;
}

bool ShowCubeDialog(wxWindow *parent, CubeRequest &outRequest) {
  CubeRequest defaults;
  defaults.name = ReadProjectString("primitive_cube_default_name", "Cube");
  defaults.lengthMeters = ReadProjectDistance("primitive_cube_default_length_m", 1.0);
  defaults.heightMeters = ReadProjectDistance("primitive_cube_default_height_m", 1.0);
  defaults.widthMeters = ReadProjectDistance("primitive_cube_default_width_m", 1.0);
  CubeDialog dialog(parent, "Add Cube", defaults, true);
  if (dialog.ShowModal() != wxID_OK)
    return false;

  outRequest = dialog.Request();
  return true;
}

bool ShowCylinderDialog(wxWindow *parent, CylinderRequest &outRequest) {
  CylinderRequest defaults;
  defaults.name = ReadProjectString("primitive_cylinder_default_name", "Cylinder");
  defaults.topRadiusMeters = ReadProjectDistance("primitive_cylinder_default_top_radius_m", 0.5);
  defaults.bottomRadiusMeters = ReadProjectDistance("primitive_cylinder_default_bottom_radius_m", 0.5);
  defaults.heightMeters = ReadProjectDistance("primitive_cylinder_default_height_m", 1.0);
  CylinderDialog dialog(parent, "Add Cylinder", defaults, true);
  if (dialog.ShowModal() != wxID_OK)
    return false;

  outRequest = dialog.Request();
  return true;
}

bool ShowSphereEditDialog(wxWindow *parent, SphereRequest &inOutRequest,
                          PrimitivePlacementRequest &inOutPlacement,
                          const PrimitiveApplyCallback &applyCallback) {
  SphereDialog dialog(parent, "Edit Sphere", inOutRequest, false,
                      &inOutPlacement, &inOutRequest, applyCallback);
  if (dialog.ShowModal() != wxID_OK)
    return false;

  inOutRequest = dialog.Request();
  inOutPlacement = dialog.Placement();
  return true;
}

bool ShowCubeEditDialog(wxWindow *parent, CubeRequest &inOutRequest,
                        PrimitivePlacementRequest &inOutPlacement,
                        const PrimitiveApplyCallback &applyCallback) {
  CubeDialog dialog(parent, "Edit Cube", inOutRequest, false,
                    &inOutPlacement, &inOutRequest, applyCallback);
  if (dialog.ShowModal() != wxID_OK)
    return false;

  inOutRequest = dialog.Request();
  inOutPlacement = dialog.Placement();
  return true;
}

bool ShowCylinderEditDialog(wxWindow *parent, CylinderRequest &inOutRequest,
                            PrimitivePlacementRequest &inOutPlacement,
                            const PrimitiveApplyCallback &applyCallback) {
  CylinderDialog dialog(parent, "Edit Cylinder", inOutRequest, false,
                        &inOutPlacement, &inOutRequest, applyCallback);
  if (dialog.ShowModal() != wxID_OK)
    return false;

  inOutRequest = dialog.Request();
  inOutPlacement = dialog.Placement();
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

Matrix BuildCylinderScaleTransform(double radiusMeters, double heightMeters) {
  const float radialScale = static_cast<float>(
      std::max(radiusMeters, 0.01) * 2.0 * kMetersToMillimeters /
      kPrimitiveCylinderDiameterMillimeters);
  const float heightScale = static_cast<float>(
      std::max(heightMeters, 0.01) * kMetersToMillimeters /
      kPrimitiveCylinderHeightMillimeters);

  Matrix transform;
  transform.u = {radialScale, 0.0f, 0.0f};
  transform.v = {0.0f, radialScale, 0.0f};
  transform.w = {0.0f, 0.0f, heightScale};
  return transform;
}

CylinderRequest ParseCylinderPrimitiveToken(const std::string &token,
                                            const Matrix &fallbackTransform) {
  CylinderRequest request;
  const double fallbackRadius =
      std::max((AxisLength(fallbackTransform.u) + AxisLength(fallbackTransform.v)) * 0.25, 0.01);
  const double fallbackHeight = std::max(AxisLength(fallbackTransform.w), 0.01);

  request.topRadiusMeters = ClampDimensionMeters(ParseFloatTokenValue(
      token, "top", fallbackRadius * kMetersToMillimeters) /
                                           kMetersToMillimeters);
  request.bottomRadiusMeters = ClampDimensionMeters(ParseFloatTokenValue(
      token, "bottom", fallbackRadius * kMetersToMillimeters) /
                                              kMetersToMillimeters);
  request.heightMeters =
      ClampDimensionMeters(ParseFloatTokenValue(
                               token, "height",
                               fallbackHeight * kMetersToMillimeters) /
                           kMetersToMillimeters);
  return request;
}

std::string BuildCylinderPrimitiveToken(double topRadiusMeters,
                                        double bottomRadiusMeters,
                                        double heightMeters) {
  const double topRadiusMm =
      ClampDimensionMeters(topRadiusMeters) * kMetersToMillimeters;
  const double bottomRadiusMm =
      ClampDimensionMeters(bottomRadiusMeters) * kMetersToMillimeters;
  const double heightMm = ClampDimensionMeters(heightMeters) * kMetersToMillimeters;

  return "primitive:cylinder;top=" + std::to_string(topRadiusMm) +
         ";bottom=" + std::to_string(bottomRadiusMm) +
         ";height=" + std::to_string(heightMm);
}

} // namespace scene_object_primitives
