/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "gdtf_download_queue_dialog.h"
#include "gdtf_resolution_status_style.h"

#include <cassert>
#include <stdexcept>
#include <vector>
#include <wx/app.h>
#include <wx/button.h>
#include <wx/gauge.h>
#include <wx/listctrl.h>
#include <wx/stattext.h>

class TestApp : public wxApp {
public:
  bool OnInit() override { return true; }
};
wxIMPLEMENT_APP_NO_MAIN(TestApp);

template <typename T> T *Find(wxWindow &parent) {
  for (auto *child : parent.GetChildren())
    if (auto *control = dynamic_cast<T *>(child))
      return control;
  assert(false);
  return nullptr;
}

void Press(wxWindow &parent, int id) {
  auto *button = parent.FindWindow(id);
  wxCommandEvent event(wxEVT_BUTTON, id);
  event.SetEventObject(button);
  button->GetEventHandler()->ProcessEvent(event);
}

void Escape(wxWindow &window) {
  wxKeyEvent event(wxEVT_CHAR_HOOK);
  event.m_keyCode = WXK_ESCAPE;
  window.GetEventHandler()->ProcessEvent(event);
}

void CheckQueue(bool autoClose, bool cancel, int action) {
  GdtfDownloadQueueDialog dialog(nullptr, autoClose);
  bool resumed = false;
  bool acknowledged = false;
  int calls = 0;
  const int result = dialog.Run([&]() {
    ++calls;
    assert(dialog.IsShown() && dialog.IsModal());
    assert(!dialog.FindWindow(wxID_OK)->IsEnabled());
    dialog.AddRow("fixture");
    dialog.AddRow("fallback");
    dialog.AddRow("pending");
    // A premature acknowledgement must leave the workflow active.
    Press(dialog, wxID_OK);
    assert(dialog.IsShown() && !dialog.IsFinished());
    if (cancel) {
      if (action == 1)
        assert(!dialog.Close());
      else if (action == 2)
        Escape(dialog);
      else
        Press(dialog, wxID_CANCEL);
      assert(dialog.IsCancellationRequested());
      assert(!dialog.FindWindow(wxID_CANCEL)->IsEnabled());
      assert(dialog.IsShown() && !dialog.IsFinished());
    }
    // Already downloaded fixtures survive cancellation of remaining work.
    dialog.Progress("fixture") = {1024, 1024};
    dialog.UpdateStatusRow("fixture", "fixture.gdtf", "Success", "1 KB / 1 KB",
                           "Downloaded and assigned",
                           DownloadRowState::Downloaded);
    dialog.UpdateStatusRow("fallback", "-", "Fallback to MVR", "0 B / ? B",
                           "No catalog match", DownloadRowState::Fallback);
    dialog.UpdateStatusRow(
        "pending", "-", cancel ? "Canceled" : "Fallback to App", "0 B / ? B",
        "Kept available fallback",
        cancel ? DownloadRowState::Canceled : DownloadRowState::Fallback);
    dialog.SetPhase(cancel ? "Queue canceled. Keeping downloaded fixtures."
                           : "Queue finished.");
    dialog.UpdateProgressGauge();
    auto *list = Find<wxListCtrl>(dialog);
    std::vector<wxString> rows;
    std::vector<wxColour> colours;
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 5; ++col)
        rows.push_back(list->GetItemText(row, col));
      colours.push_back(list->GetItemTextColour(row));
    }
    assert(colours[0] ==
           GdtfResolutionStatusColour(
               rider_fixture_resolution::StatusSemantic::Success));
    const int gauge = Find<wxGauge>(dialog)->GetValue();
    if (!autoClose) {
      // This event can only run after work returns and Finish enables OK.
      dialog.CallAfter([&, list, rows, colours, gauge]() {
        assert(!resumed && dialog.IsShown() && dialog.IsFinished());
        assert(dialog.FindWindow(wxID_OK)->IsEnabled());
        assert(!dialog.FindWindow(wxID_CANCEL)->IsEnabled());
        assert(list->GetItemCount() == 3);
        for (int row = 0; row < 3; ++row) {
          for (int col = 0; col < 5; ++col)
            assert(rows[row * 5 + col] == list->GetItemText(row, col));
          assert(colours[row] == list->GetItemTextColour(row));
        }
        assert(Find<wxGauge>(dialog)->GetValue() == gauge);
        bool summary = false, bytes = false, phase = false;
        for (auto *child : dialog.GetChildren()) {
          if (auto *text = dynamic_cast<wxStaticText *>(child)) {
            summary |=
                text->GetLabel().Contains("3/3 processed  |  1 downloaded");
            bytes |= text->GetLabel().Contains("1.0 KB / ? B");
            phase |= text->GetLabel().Contains("Queue finished.") ||
                     text->GetLabel().Contains("Queue canceled.");
          }
        }
        assert(summary && bytes && phase);
        acknowledged = true;
        if (action == 1)
          dialog.Close();
        else if (action == 2)
          Escape(dialog);
        else
          Press(dialog, wxID_OK);
      });
    }
  });
  resumed = true;
  assert(calls == 1 && result == wxID_OK);
  assert(dialog.IsFinished() && !dialog.IsShown());
  assert(autoClose || acknowledged);
  assert(dialog.IsCancellationRequested() == cancel);
}

int main(int argc, char **argv) {
  assert(wxEntryStart(argc, argv));
  assert(wxTheApp->CallOnInit());
  wxTheApp->SetExitOnFrameDelete(false);
  for (bool autoClose : {false, true})
    for (bool cancel : {false, true})
      for (int action : {0, 1, 2})
        CheckQueue(autoClose, cancel, action);
  {
    GdtfDownloadQueueDialog dialog(nullptr, false);
    bool caught = false;
    try {
      dialog.Run([] { throw std::runtime_error("workflow failed"); });
    } catch (const std::runtime_error &) {
      caught = true;
    }
    assert(caught && !dialog.IsShown());
  }
  wxTheApp->OnExit();
  wxEntryCleanup();
}
