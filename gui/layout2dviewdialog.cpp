#include "layout2dviewdialog.h"

#include "viewer2dpanel.h"
#include "viewer2drenderpanel.h"

#include <wx/button.h>
#include <wx/sizer.h>

Layout2DViewDialog::Layout2DViewDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "Editar vista 2D", wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
  auto *mainSizer = new wxBoxSizer(wxVERTICAL);
  auto *contentSizer = new wxBoxSizer(wxHORIZONTAL);

  viewerPanel = new Viewer2DPanel(this);
  renderPanel = new Viewer2DRenderPanel(this);

  contentSizer->Add(viewerPanel, 1, wxEXPAND | wxALL, 8);
  contentSizer->Add(renderPanel, 0, wxEXPAND | wxTOP | wxBOTTOM | wxRIGHT, 8);

  mainSizer->Add(contentSizer, 1, wxEXPAND);

  auto *buttonSizer = new wxStdDialogButtonSizer();
  auto *okButton = new wxButton(this, wxID_OK, "OK");
  auto *cancelButton = new wxButton(this, wxID_CANCEL, "Cancel");
  okButton->Bind(wxEVT_BUTTON, &Layout2DViewDialog::OnOk, this);
  cancelButton->Bind(wxEVT_BUTTON, &Layout2DViewDialog::OnCancel, this);
  buttonSizer->AddButton(okButton);
  buttonSizer->AddButton(cancelButton);
  buttonSizer->Realize();

  mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 8);

  SetSizerAndFit(mainSizer);
  SetMinSize(wxSize(900, 600));
  CentreOnParent();
}

void Layout2DViewDialog::OnOk(wxCommandEvent &event) {
  EndModal(wxID_OK);
  event.Skip();
}

void Layout2DViewDialog::OnCancel(wxCommandEvent &event) {
  EndModal(wxID_CANCEL);
  event.Skip();
}
