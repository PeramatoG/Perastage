/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "patch/dmx_patch_canvas.h"
#include <wx/dcbuffer.h>
#include <wx/settings.h>
#include <wx/control.h>
#include <wx/dcclient.h>
#include <algorithm>

namespace {
wxString Text(const std::string &value) { return wxString::FromUTF8(value); }
wxString FitLabel(wxDC &dc, wxString value, int width) {
  return wxControl::Ellipsize(value, dc, wxELLIPSIZE_END, width);
}
using dmx_patch_ui::FixtureIds;

bool SameFixture(const dmx_patch::Cell &a, const dmx_patch::Cell &b) {
  return !FixtureIds(a).empty() && FixtureIds(a) == FixtureIds(b);
}
}
DmxPatchCanvas::DmxPatchCanvas(wxWindow *parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxHSCROLL | wxVSCROLL | wxBORDER_NONE) {
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  Bind(wxEVT_PAINT, &DmxPatchCanvas::Paint, this);
  Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &event) {
    SetFocus();
    if (pick_) pick_(FixturesAt(event.GetPosition()), event.ShiftDown(), event.ControlDown());
  });
  Bind(wxEVT_MOTION, &DmxPatchCanvas::Hover, this);
  Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent &e) { hovered_.reset(); UnsetToolTip(); e.Skip(); });
  Bind(wxEVT_DPI_CHANGED, [this](wxDPIChangedEvent &e) { UpdateMetrics(); e.Skip(); });
  Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent &e) { Refresh(); e.Skip(); });
  Bind(wxEVT_SIZE, [this](wxSizeEvent &e) { UpdateMetrics(); e.Skip(); });
  UpdateMetrics();
}
void DmxPatchCanvas::UpdateMetrics() {
  auto font = GetFont(); font.SetPointSize(std::max(7, font.GetPointSize() - 1));
  wxClientDC dc(this); dc.SetFont(font);
  const int minimum = FromDIP(52), available = std::max(1, GetClientSize().x);
  geometry_.columns = std::clamp(available / minimum, 1, 512);
  geometry_.width = std::max(minimum, available / geometry_.columns);
  geometry_.height = std::max(FromDIP(42), 3 * dc.GetCharHeight() + 2 * FromDIP(2));
  SetScrollRate(FromDIP(12), FromDIP(12));
  const int rows = (512 + geometry_.columns - 1) / geometry_.columns;
  const wxSize size(geometry_.columns * geometry_.width, rows * geometry_.height);
  if (GetVirtualSize() != size) SetVirtualSize(size);
  hovered_.reset(); UnsetToolTip(); Refresh();
}

void DmxPatchCanvas::SetSnapshot(std::shared_ptr<const dmx_patch::Snapshot> snapshot) {
  snapshot_ = std::move(snapshot);
  if (snapshot_) blocks_.emplace(*snapshot_); else blocks_.reset();
  hovered_.reset(); UnsetToolTip(); Refresh();
}
std::optional<dmx_patch::Address> DmxPatchCanvas::AddressAt(const wxPoint &point) const {
  const auto virtualPoint = CalcUnscrolledPosition(point);
  const auto channel = geometry_.Hit(virtualPoint.x, virtualPoint.y);
  if (!channel || !snapshot_) return {};
  return dmx_patch::Address{snapshot_->universe, *channel};
}
std::vector<std::string> DmxPatchCanvas::FixturesAt(const wxPoint &point) const {
  const auto address = AddressAt(point);
  if (!address) return {};
  const auto ids = FixtureIds(snapshot_->cells[address->channel - 1]);
  return {ids.begin(), ids.end()};
}
void DmxPatchCanvas::Paint(wxPaintEvent &) {
  wxAutoBufferedPaintDC dc(this);
  dc.SetBackground(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW))); dc.Clear();
  PrepareDC(dc);
  if (!snapshot_) return;
  auto font = GetFont(); font.SetPointSize(std::max(7, font.GetPointSize() - 1)); dc.SetFont(font);
  const int pad = FromDIP(2), line = dc.GetCharHeight();
  const auto neutral = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
  const auto ink = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
  const auto border = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
  const int w = geometry_.width, h = geometry_.height;
  const auto top = CalcUnscrolledPosition(wxPoint(0, 0));
  const int firstRow = std::max(0, top.y / h);
  const int lastRow = std::min((512 + geometry_.columns - 1) / geometry_.columns, (top.y + GetClientSize().y) / h + 1);
  for (int row = firstRow; row < lastRow; ++row) {
    for (int col = 0; col < geometry_.columns; ++col) {
      const int i = row * geometry_.columns + col;
      if (i >= 512) break;
      const auto &cell = snapshot_->cells[i];
      const auto &entries = cell.occupants.empty() ? cell.provisional : cell.occupants;
      const bool start = blocks_->Starts(i), end = blocks_->Ends(i);
      wxColour color = neutral, text = ink;
      if (!entries.empty()) {
        const auto &info = snapshot_->fixtures.at(entries.front().uuid);
        if (info.colorHex) color = wxColour(Text(*info.colorHex));
        if (!color.IsOk()) color = neutral;
        text = (color.Red() * 299 + color.Green() * 587 + color.Blue() * 114 < 140000) ? *wxWHITE : *wxBLACK;
      }
      const wxRect rect(col * w, row * h, w, h);
      const int radius = std::min(FromDIP(18), std::min(w, h) / 2 - 1);
      dc.SetBrush(wxBrush(color)); dc.SetPen(wxPen(border));
      if (start || end) {
        dc.DrawRoundedRectangle(rect, radius);
        // Square the lower corners and the top corner not marking a boundary.
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(rect.x, rect.y + radius, w, h - radius);
        if (!start) dc.DrawRectangle(rect.x, rect.y, radius + 1, radius + 1);
        if (!end) dc.DrawRectangle(rect.GetRight() - radius, rect.y, radius + 1, radius + 1);
        dc.SetPen(wxPen(border));
        dc.DrawLine(rect.x + (start ? radius : 0), rect.y, rect.GetRight() - (end ? radius : 0), rect.y);
        dc.DrawLine(rect.x, rect.GetBottom(), rect.GetRight(), rect.GetBottom());
        dc.DrawLine(rect.x, rect.y + radius, rect.x, rect.GetBottom());
        dc.DrawLine(rect.GetRight(), rect.y + radius, rect.GetRight(), rect.GetBottom());
      } else dc.DrawRectangle(rect);
      if (!cell.provisional.empty()) {
        dc.SetPen(wxPen(wxColour(255, 180, 0), FromDIP(2), wxPENSTYLE_SHORT_DASH));
        dc.SetBrush(*wxTRANSPARENT_BRUSH); dc.DrawRectangle(wxRect(rect).Deflate(1));
      }
      if (cell.Conflict()) {
        dc.SetPen(wxPen(wxColour(255, 190, 0), FromDIP(2)));
        // Hatch only the number band, retaining readable function and name bands.
        for (int x = pad; x < w - pad; x += FromDIP(9))
          dc.DrawLine(rect.x + x, rect.y + 1, rect.x + std::min(w - 1, x + FromDIP(8)), rect.y + pad + line);
        dc.SetBrush(*wxTRANSPARENT_BRUSH); dc.DrawRectangle(wxRect(rect).Deflate(1));
      }
      dc.SetTextForeground(text);
      const auto number = wxString::Format("%d", i + 1);
      const int numberWidth = dc.GetTextExtent(number).x;
      const int numberX = rect.x + (w - numberWidth) / 2;
      if (start) {
        dc.SetBrush(*wxTRANSPARENT_BRUSH); dc.SetPen(wxPen(text, FromDIP(1)));
        dc.DrawRectangle(numberX - pad, rect.y + 1, numberWidth + 2 * pad, line + 1);
      }
      dc.DrawText(number, numberX, rect.y + pad);
      if (!cell.provisional.empty())
        dc.DrawText("?", rect.GetRight() - pad - dc.GetTextExtent("?").x, rect.y + pad);
      if (!entries.empty()) {
        const auto &entry = entries.front();
        wxString label = Text(entry.attribute.empty() ? entry.functions : entry.attribute);
        if (entry.byteIndex == 1) label += _(" Fine");
        else if (entry.byteIndex > 1) label += wxString::Format(_(" Byte %d"), entry.byteIndex + 1);
        dc.DrawText(FitLabel(dc, label, w - 2 * pad), rect.x + pad, rect.y + h - line - pad);
      }

    }
    auto nameFont = font; nameFont.SetWeight(wxFONTWEIGHT_BOLD); dc.SetFont(nameFont);
    // Name each fixture on its first displayed row only; wrapping never repeats it.
    for (int col = 0; col < geometry_.columns;) {
      const int i = row * geometry_.columns + col;
      if (i >= 512) break;
      const auto &cell = snapshot_->cells[i];
      const auto namesHere = blocks_->NamesAt(i);
      if (namesHere.empty()) { ++col; continue; }
      int end = col + 1;
      while (end < geometry_.columns && row * geometry_.columns + end < 512 && SameFixture(cell, snapshot_->cells[row * geometry_.columns + end])) ++end;
      const auto &info = snapshot_->fixtures.at(namesHere.front());
      wxColour bg = info.colorHex ? wxColour(Text(*info.colorHex)) : neutral;
      dc.SetTextForeground(bg.IsOk() && bg.Red() * 299 + bg.Green() * 587 + bg.Blue() * 114 < 140000 ? *wxWHITE : *wxBLACK);
      wxString names;
      for (const auto &uuid : namesHere) {
        if (!names.empty()) names += " / ";
        names += Text(snapshot_->fixtures.at(uuid).name);
      }
      dc.DrawText(FitLabel(dc, names, (end - col) * w - 2 * pad), col * w + pad, row * h + pad + line);
      col = end;
    }
    dc.SetFont(font);
  }
  PaintHighlights(dc, firstRow, lastRow);
}
wxString DmxPatchCanvas::Tooltip(int channel) const {
  wxString result = wxString::Format(_("Universe %s | Channel %d"), Text(std::to_string(snapshot_->universe)), channel);
  const auto &cell = snapshot_->cells[channel - 1];
  if (cell.Conflict()) result += "\n" + _("Overlapping fixtures");
  for (const auto &entry : cell.occupants) {
    const auto &info = snapshot_->fixtures.at(entry.uuid);
    result += wxString::Format(_("\n%s - %s\nUUID: %s\nGDTF break %d / MVR break %d | Offset %d | Byte %d\nAttribute: %s\nFunctions: %s"),
        Text(info.identifier), Text(info.name), Text(entry.uuid), entry.gdtfBreak, entry.gdtfBreak - 1,
        entry.offset, entry.byteIndex + 1, Text(entry.attribute), Text(entry.functions));
    if (!info.purpose.empty()) result += "\n" + wxString::Format(_("Fixture purpose: %s"), Text(info.purpose));
  }
  for (const auto &entry : cell.provisional) {
    const auto &info = snapshot_->fixtures.at(entry.uuid);
    result += "\n" + wxString::Format(_("PROVISIONAL: %s | Declared offset %d | GDTF break: %s"),
        Text(info.name), entry.offset, entry.gdtfBreak ? Text(std::to_string(entry.gdtfBreak)) : _("Unresolved"));
    result += "\n" + _("Declared-offset compatibility preview; this is not a resolved physical patch assignment.");
    result += "\n" + Text(entry.attribute) + " | " + Text(entry.functions);
  }
  return result;
}
void DmxPatchCanvas::Hover(wxMouseEvent &event) {
  const auto address = AddressAt(event.GetPosition());
  const std::optional<int> channel = address ? std::optional<int>(address->channel) : std::nullopt;
  if (channel != hovered_) {
    hovered_ = channel;
    if (channel) SetToolTip(Tooltip(*channel)); else UnsetToolTip();
  }
  event.Skip();
}
