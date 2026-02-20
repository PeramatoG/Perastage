#pragma once

#include "symbols/Symbol2DTypes.h"

#include <wx/frame.h>
#include <wx/panel.h>

class SymbolPreviewPanel : public wxPanel {
public:
  SymbolPreviewPanel(wxWindow *parent, symbols::SymbolCollection symbols);

private:
  void OnPaint(wxPaintEvent &event);

  symbols::SymbolCollection symbols_;
};

class SymbolPreviewWindow : public wxFrame {
public:
  SymbolPreviewWindow(wxWindow *parent, const symbols::SymbolCollection &symbols);
};
