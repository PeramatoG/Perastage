#pragma once

#include <array>

#include <wx/frame.h>
#include <wx/image.h>

class SymbolPreviewWindow : public wxFrame {
public:
  SymbolPreviewWindow(wxWindow *parent,
                      const std::array<wxImage, 4> &images);

private:
  void OnPaint(wxPaintEvent &event);
  void DrawCell(wxDC &dc, const wxRect &cell, const wxImage &image,
                const wxString &label);

  std::array<wxImage, 4> images_;
};
