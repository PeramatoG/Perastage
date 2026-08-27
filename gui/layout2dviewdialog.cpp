#include "layout2dviewdialog.h"

#include "configmanager.h"
#include "editable_focus_utils.h"
#include "layerpanel.h"
#include "summarypanel.h"
#include "viewer2dpanel.h"
#include "viewer2drenderpanel.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <wx/button.h>
#include <wx/slider.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace {

// Reports whether the key event requests a viewer fit without command modifiers.
bool IsFitViewShortcut(const wxKeyEvent &event) {
  if (event.ControlDown() || event.AltDown() || event.MetaDown())
    return false;

  const int keyCode = event.GetKeyCode();
  return keyCode == 'Z' || keyCode == 'z';
}

} // namespace

// Builds the modal editor for a layout 2D view.
Layout2DViewDialog::Layout2DViewDialog(wxWindow *parent,
                                       ConfigManager *visibilityConfig,
                                       ConfigManager *colorConfig)
    : wxDialog(parent, wxID_ANY, "2D View Editor", wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER |
                                   wxMAXIMIZE_BOX | wxMINIMIZE_BOX) {
  auto *mainSizer = new wxBoxSizer(wxVERTICAL);
  auto *contentSizer = new wxBoxSizer(wxHORIZONTAL);

  viewerPanel = new Viewer2DPanel(this, false, false, false);
  renderPanel = new Viewer2DRenderPanel(this);
  layerPanel = new LayerPanel(this, false, visibilityConfig,
                              gui::InitialPopulationPolicy::Deferred);
  summaryPanel = new SummaryPanel(this, visibilityConfig, colorConfig);

  renderPanel->SetMinSize(wxSize(240, -1));
  layerPanel->SetMinSize(wxSize(290, 220));
  summaryPanel->SetMinSize(wxSize(290, 220));

  auto *rightColumnSizer = new wxBoxSizer(wxVERTICAL);
  rightColumnSizer->Add(layerPanel, 1, wxEXPAND | wxBOTTOM, 8);
  rightColumnSizer->Add(summaryPanel, 1, wxEXPAND);

  contentSizer->Add(viewerPanel, 1, wxEXPAND | wxALL, 8);
  contentSizer->Add(renderPanel, 0, wxEXPAND | wxTOP | wxBOTTOM | wxRIGHT, 8);
  contentSizer->Add(rightColumnSizer, 0, wxEXPAND | wxTOP | wxBOTTOM | wxRIGHT, 8);

  mainSizer->Add(contentSizer, 1, wxEXPAND);

  auto *scaleSizer = new wxBoxSizer(wxHORIZONTAL);
  auto *scaleLabel = new wxStaticText(this, wxID_ANY, _("Frame scale"));
  scaleSlider = new wxSlider(this, wxID_ANY, 100, 25, 300);
  scaleValueLabel = new wxStaticText(this, wxID_ANY, _("100%"));
  scaleSlider->Bind(wxEVT_SLIDER, &Layout2DViewDialog::OnScaleChanged, this);

  scaleSizer->Add(scaleLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  scaleSizer->Add(scaleSlider, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  scaleSizer->Add(scaleValueLabel, 0, wxALIGN_CENTER_VERTICAL);
  mainSizer->Add(scaleSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  auto *buttonSizer = new wxStdDialogButtonSizer();
  auto *okButton = new wxButton(this, wxID_OK, _("OK"));
  auto *cancelButton = new wxButton(this, wxID_CANCEL, _("Cancel"));
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
  Bind(wxEVT_CHAR_HOOK, &Layout2DViewDialog::OnCharHook, this);
}

// Accepts the edited layout 2D view state.
void Layout2DViewDialog::OnOk(wxCommandEvent &event) {
  EndModal(wxID_OK);
  event.Skip();
}

// Cancels the layout 2D view editing session.
void Layout2DViewDialog::OnCancel(wxCommandEvent &event) {
  EndModal(wxID_CANCEL);
  event.Skip();
}

// Synchronizes the viewer and side panels when the dialog is shown.
void Layout2DViewDialog::OnShow(wxShowEvent &event) {
  if (event.IsShown() && !initialShowSyncDone) {
    initialShowSyncDone = true;
    if (layerPanel) {
      layerPanel->ReloadLayers();
    }
    if (summaryPanel) {
      summaryPanel->ShowFixtureSummary();
    }
    if (viewerPanel) {
      auto retries = std::make_shared<int>(3);
      std::shared_ptr<std::function<void()>> syncRender =
          std::make_shared<std::function<void()>>();
      *syncRender = [panel = viewerPanel, retries, syncRender]() {
        if (!panel)
          return;

        int width = 0;
        int height = 0;
        panel->GetClientSize(&width, &height);
        if (width <= 0 || height <= 0) {
          if (*retries > 0) {
            --(*retries);
            panel->CallAfter(*syncRender);
          }
          return;
        }

        panel->UpdateScene(true);
        panel->Refresh();
        panel->Update();
      };
      (*syncRender)();
    }
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

// Routes local viewport shortcuts to the embedded 2D viewport.
void Layout2DViewDialog::OnCharHook(wxKeyEvent &event) {
  if (!viewerPanel || gui::IsEditableWidgetFocused(wxWindow::FindFocus())) {
    event.Skip();
    return;
  }

  if (IsFitViewShortcut(event) && viewerPanel->FitViewToScene())
    return;

  if (viewerPanel->TryHandleViewportNavigationKey(event.GetKeyCode(),
                                                 event.AltDown())) {
    return;
  }

  event.Skip();
}

// Applies the selected layout frame scale to the embedded viewer.
void Layout2DViewDialog::OnScaleChanged(wxCommandEvent &event) {
  if (viewerPanel && scaleSlider) {
    const float scale =
        static_cast<float>(scaleSlider->GetValue()) / 100.0f;
    viewerPanel->SetLayoutEditOverlayScale(scale);
  }
  UpdateScaleLabel();
  event.Skip();
}

// Refreshes the frame scale percentage label.
void Layout2DViewDialog::UpdateScaleLabel() {
  if (!scaleValueLabel || !scaleSlider)
    return;
  scaleValueLabel->SetLabel(
      wxString::Format(_("%d%%"), scaleSlider->GetValue()));
}
