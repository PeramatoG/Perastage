#pragma once

#include <wx/frame.h>
#include <wx/image.h>

class SymbolPreviewWindow : public wxFrame {
public:
  SymbolPreviewWindow(wxWindow *parent, const wxImage &image);

private:
  void OnPaint(wxPaintEvent &event);

  wxImage image_;
};
