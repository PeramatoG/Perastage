#include "mainwindow.h"

#include <chrono>
#include <string>

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

#include "configmanager.h"
#include "guiconfigservices.h"
#include "layer.h"
#include "matrixutils.h"
#include "sceneobjecttablepanel.h"
#include "viewer3dpanel.h"

namespace {

class AddSpherePrimitiveDialog final : public wxDialog {
public:
  explicit AddSpherePrimitiveDialog(wxWindow *parent)
      : wxDialog(parent, wxID_ANY, "Add Sphere Primitive", wxDefaultPosition,
                 wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    auto *mainSizer = new wxBoxSizer(wxVERTICAL);
    auto *grid = new wxFlexGridSizer(2, 2, 8, 8);
    grid->AddGrowableCol(1, 1);

    grid->Add(new wxStaticText(this, wxID_ANY, "Radius (m):"),
              0, wxALIGN_CENTER_VERTICAL);
    radiusCtrl = new wxSpinCtrlDouble(this, wxID_ANY);
    radiusCtrl->SetRange(0.01, 1000.0);
    radiusCtrl->SetIncrement(0.1);
    radiusCtrl->SetDigits(3);
    radiusCtrl->SetValue(1.0);
    grid->Add(radiusCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, "Units:"), 0,
              wxALIGN_CENTER_VERTICAL);
    quantityCtrl = new wxSpinCtrl(this, wxID_ANY);
    quantityCtrl->SetRange(1, 1000);
    quantityCtrl->SetValue(1);
    grid->Add(quantityCtrl, 1, wxEXPAND);

    mainSizer->Add(grid, 1, wxALL | wxEXPAND, 12);
    mainSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL),
                   0, wxALL | wxEXPAND, 12);
    SetSizerAndFit(mainSizer);
  }

  double RadiusMeters() const { return radiusCtrl->GetValue(); }
  int Quantity() const { return quantityCtrl->GetValue(); }

private:
  wxSpinCtrlDouble *radiusCtrl = nullptr;
  wxSpinCtrl *quantityCtrl = nullptr;
};

class AddCubePrimitiveDialog final : public wxDialog {
public:
  explicit AddCubePrimitiveDialog(wxWindow *parent)
      : wxDialog(parent, wxID_ANY, "Add Cube Primitive", wxDefaultPosition,
                 wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    auto *mainSizer = new wxBoxSizer(wxVERTICAL);
    auto *grid = new wxFlexGridSizer(2, 4, 8, 8);
    grid->AddGrowableCol(1, 1);

    grid->Add(new wxStaticText(this, wxID_ANY, "Length (m):"), 0,
              wxALIGN_CENTER_VERTICAL);
    lengthCtrl = new wxSpinCtrlDouble(this, wxID_ANY);
    lengthCtrl->SetRange(0.01, 1000.0);
    lengthCtrl->SetIncrement(0.1);
    lengthCtrl->SetDigits(3);
    lengthCtrl->SetValue(1.0);
    grid->Add(lengthCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, "Width (m):"), 0,
              wxALIGN_CENTER_VERTICAL);
    widthCtrl = new wxSpinCtrlDouble(this, wxID_ANY);
    widthCtrl->SetRange(0.01, 1000.0);
    widthCtrl->SetIncrement(0.1);
    widthCtrl->SetDigits(3);
    widthCtrl->SetValue(1.0);
    grid->Add(widthCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, "Height (m):"), 0,
              wxALIGN_CENTER_VERTICAL);
    heightCtrl = new wxSpinCtrlDouble(this, wxID_ANY);
    heightCtrl->SetRange(0.01, 1000.0);
    heightCtrl->SetIncrement(0.1);
    heightCtrl->SetDigits(3);
    heightCtrl->SetValue(1.0);
    grid->Add(heightCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, "Units:"), 0,
              wxALIGN_CENTER_VERTICAL);
    quantityCtrl = new wxSpinCtrl(this, wxID_ANY);
    quantityCtrl->SetRange(1, 1000);
    quantityCtrl->SetValue(1);
    grid->Add(quantityCtrl, 1, wxEXPAND);

    mainSizer->Add(grid, 1, wxALL | wxEXPAND, 12);
    mainSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL),
                   0, wxALL | wxEXPAND, 12);
    SetSizerAndFit(mainSizer);
  }

  double LengthMeters() const { return lengthCtrl->GetValue(); }
  double WidthMeters() const { return widthCtrl->GetValue(); }
  double HeightMeters() const { return heightCtrl->GetValue(); }
  int Quantity() const { return quantityCtrl->GetValue(); }

private:
  wxSpinCtrlDouble *lengthCtrl = nullptr;
  wxSpinCtrlDouble *widthCtrl = nullptr;
  wxSpinCtrlDouble *heightCtrl = nullptr;
  wxSpinCtrl *quantityCtrl = nullptr;
};

Matrix BuildPrimitiveLocalTransformMm(float sizeXmm, float sizeYmm,
                                      float sizeZmm) {
  Matrix local = MatrixUtils::Identity();
  local.u = {sizeXmm / 1000.0f, 0.0f, 0.0f};
  local.v = {0.0f, sizeYmm / 1000.0f, 0.0f};
  local.w = {0.0f, 0.0f, sizeZmm / 1000.0f};
  return local;
}

} // namespace

void MainWindow::OnAddSpherePrimitive(wxCommandEvent &WXUNUSED(event)) {
  AddSpherePrimitiveDialog dialog(this);
  if (dialog.ShowModal() != wxID_OK)
    return;

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  auto &scene = cfg.GetScene();
  cfg.PushUndoState("add sphere primitive");

  const float diameterMm = static_cast<float>(dialog.RadiusMeters() * 2000.0);
  const long quantity = dialog.Quantity();
  const auto baseId = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::string layerName = cfg.GetCurrentLayer();

  bool hasLayer = false;
  for (const auto &[uid, layer] : scene.layers) {
    if (layer.name == layerName) {
      hasLayer = true;
      break;
    }
  }
  if (!hasLayer) {
    Layer layer;
    layer.uuid = wxString::Format("layer_%lld", static_cast<long long>(baseId))
                     .ToStdString();
    layer.name = layerName;
    scene.layers[layer.uuid] = layer;
  }

  for (long i = 0; i < quantity; ++i) {
    SceneObject object;
    object.uuid = wxString::Format("uuid_%lld", static_cast<long long>(baseId + i))
                      .ToStdString();
    object.name = quantity > 1 ? "Sphere " + std::to_string(i + 1) : "Sphere";
    object.layer = layerName;
    object.primitiveType = "sphere";
    object.primitiveSizeMm = {diameterMm, diameterMm, diameterMm};

    GeometryInstance geo;
    geo.localTransform = BuildPrimitiveLocalTransformMm(
        diameterMm, diameterMm, diameterMm);
    object.geometries.push_back(geo);

    scene.sceneObjects[object.uuid] = object;
  }

  if (sceneObjPanel)
    sceneObjPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

void MainWindow::OnAddCubePrimitive(wxCommandEvent &WXUNUSED(event)) {
  AddCubePrimitiveDialog dialog(this);
  if (dialog.ShowModal() != wxID_OK)
    return;

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  auto &scene = cfg.GetScene();
  cfg.PushUndoState("add cube primitive");

  const float lengthMm = static_cast<float>(dialog.LengthMeters() * 1000.0);
  const float widthMm = static_cast<float>(dialog.WidthMeters() * 1000.0);
  const float heightMm = static_cast<float>(dialog.HeightMeters() * 1000.0);
  const long quantity = dialog.Quantity();
  const auto baseId = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::string layerName = cfg.GetCurrentLayer();

  bool hasLayer = false;
  for (const auto &[uid, layer] : scene.layers) {
    if (layer.name == layerName) {
      hasLayer = true;
      break;
    }
  }
  if (!hasLayer) {
    Layer layer;
    layer.uuid = wxString::Format("layer_%lld", static_cast<long long>(baseId))
                     .ToStdString();
    layer.name = layerName;
    scene.layers[layer.uuid] = layer;
  }

  for (long i = 0; i < quantity; ++i) {
    SceneObject object;
    object.uuid = wxString::Format("uuid_%lld", static_cast<long long>(baseId + i))
                      .ToStdString();
    object.name = quantity > 1 ? "Cube " + std::to_string(i + 1) : "Cube";
    object.layer = layerName;
    object.primitiveType = "cube";
    object.primitiveSizeMm = {lengthMm, widthMm, heightMm};

    GeometryInstance geo;
    geo.localTransform =
        BuildPrimitiveLocalTransformMm(lengthMm, widthMm, heightMm);
    object.geometries.push_back(geo);

    scene.sceneObjects[object.uuid] = object;
  }

  if (sceneObjPanel)
    sceneObjPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
}
