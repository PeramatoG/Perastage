#include "layout2dviewdialog.h"

#include "layerpanel.h"
#include "viewer2dpanel.h"
#include "viewer2drenderpanel.h"

#include <algorithm>
#include <cmath>
#include <wx/button.h>
#include <wx/slider.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

Layout2DViewDialog::Layout2DViewDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "2D View Editor", wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER |
                                   wxMAXIMIZE_BOX | wxMINIMIZE_BOX) {
  auto *mainSizer = new wxBoxSizer(wxVERTICAL);
  auto *contentSizer = new wxBoxSizer(wxHORIZONTAL);

  viewerPanel = new Viewer2DPanel(this);
  renderPanel = new Viewer2DRenderPanel(this);
  layerPanel = new LayerPanel(this, false);

  renderPanel->SetMinSize(wxSize(260, -1));
  layerPanel->SetMinSize(wxSize(220, -1));

  contentSizer->Add(viewerPanel, 1, wxEXPAND | wxALL, 8);
  contentSizer->Add(renderPanel, 0, wxEXPAND | wxTOP | wxBOTTOM | wxRIGHT, 8);
  contentSizer->Add(layerPanel, 0, wxEXPAND | wxTOP | wxBOTTOM | wxRIGHT, 8);

  mainSizer->Add(contentSizer, 1, wxEXPAND);

  auto *scaleSizer = new wxBoxSizer(wxHORIZONTAL);
  auto *scaleLabel = new wxStaticText(this, wxID_ANY, "Frame scale");
  scaleSlider = new wxSlider(this, wxID_ANY, 100, 25, 300);
  scaleValueLabel = new wxStaticText(this, wxID_ANY, "100%");
  scaleSlider->Bind(wxEVT_SLIDER, &Layout2DViewDialog::OnScaleChanged, this);

  scaleSizer->Add(scaleLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  scaleSizer->Add(scaleSlider, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  scaleSizer->Add(scaleValueLabel, 0, wxALIGN_CENTER_VERTICAL);
  mainSizer->Add(scaleSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  auto *buttonSizer = new wxStdDialogButtonSizer();
  auto *okButton = new wxButton(this, wxID_OK, "OK");
  auto *cancelButton = new wxButton(this, wxID_CANCEL, "Cancel");
  okButton->Bind(wxEVT_BUTTON, &Layout2DViewDialog::OnOk, this);
  cancelButton->Bind(wxEVT_BUTTON, &Layout2DViewDialog::OnCancel, this);
  buttonSizer->AddButton(okButton);
  buttonSizer->AddButton(cancelButton);
  buttonSizer->Realize();

  mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 8);

  SetSizer(mainSizer);
  SetSize(wxSize(1200, 800));
  SetMinSize(wxSize(1000, 700));
  Layout();
  CentreOnParent();

  Bind(wxEVT_SHOW, &Layout2DViewDialog::OnShow, this);
}

void Layout2DViewDialog::OnOk(wxCommandEvent &event) {
  EndModal(wxID_OK);
  event.Skip();
}

void Layout2DViewDialog::OnCancel(wxCommandEvent &event) {
  EndModal(wxID_CANCEL);
  event.Skip();
}

void Layout2DViewDialog::OnShow(wxShowEvent &event) {
  if (event.IsShown() && viewerPanel) {
    viewerPanel->LoadViewFromConfig();
    viewerPanel->UpdateScene(true);
    viewerPanel->Update();
  }
  if (viewerPanel && scaleSlider) {
    const int value = static_cast<int>(
        std::lround(viewerPanel->GetLayoutEditOverlayScale() * 100.0f));
    scaleSlider->SetValue(std::clamp(value, scaleSlider->GetMin(),
                                     scaleSlider->GetMax()));
  }
  UpdateScaleLabel();
  event.Skip();
}

void Layout2DViewDialog::OnScaleChanged(wxCommandEvent &event) {
  if (viewerPanel && scaleSlider) {
    const float scale =
        static_cast<float>(scaleSlider->GetValue()) / 100.0f;
    viewerPanel->SetLayoutEditOverlayScale(scale);
  }
  UpdateScaleLabel();
  event.Skip();
}

void Layout2DViewDialog::UpdateScaleLabel() {
  if (!scaleValueLabel || !scaleSlider)
    return;
  scaleValueLabel->SetLabel(
      wxString::Format("%d%%", scaleSlider->GetValue()));
}
