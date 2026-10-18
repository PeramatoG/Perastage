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
#include <algorithm>
#include <exception>
#include <utility>
#include <wx/button.h>
#include <wx/gauge.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

static wxColour RowTextColor(DownloadRowState state) {
  switch (state) {
  case DownloadRowState::Downloaded:
    return GdtfResolutionStatusColour(
        rider_fixture_resolution::StatusSemantic::Success);
  case DownloadRowState::Fallback:
    return GdtfResolutionStatusColour(
        rider_fixture_resolution::StatusSemantic::Warning);
  case DownloadRowState::Canceled:
    return GdtfResolutionStatusColour(
        rider_fixture_resolution::StatusSemantic::Muted);
  case DownloadRowState::Downloading:
    return GdtfResolutionStatusColour(
        rider_fixture_resolution::StatusSemantic::Information);
  case DownloadRowState::Pending:
  default:
    return GdtfResolutionStatusColour(
        rider_fixture_resolution::StatusSemantic::Neutral);
  }
}
GdtfDownloadQueueDialog::GdtfDownloadQueueDialog(wxWindow *parent,
                                                 bool autoClose)
    : wxDialog(parent, wxID_ANY, _("GDTF download queue"), wxDefaultPosition,
               wxSize(1140, 580)),
      autoClose(autoClose) {
  wxBoxSizer *infoSizer = new wxBoxSizer(wxVERTICAL);
  summaryText = new wxStaticText(this, wxID_ANY,
                                 _("Selected fixture types for download"));
  wxFont summaryFont = summaryText->GetFont();
  summaryFont.SetWeight(wxFONTWEIGHT_BOLD);
  summaryText->SetFont(summaryFont);
  infoSizer->Add(summaryText, 0, wxLEFT | wxRIGHT | wxTOP, 8);
  progressPhaseText =
      new wxStaticText(this, wxID_ANY, _("Preparing download queue..."));
  progressPhaseText->SetForegroundColour(wxColour(140, 140, 140));
  infoSizer->Add(progressPhaseText, 0, wxLEFT | wxRIGHT | wxTOP, 8);
  progressGauge = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition,
                              wxSize(-1, 6), wxGA_HORIZONTAL | wxGA_SMOOTH);
  progressGauge->SetForegroundColour(wxColour(80, 145, 90));
  progressGauge->SetBackgroundColour(wxColour(52, 52, 52));
  progressGauge->SetValue(0);
  infoSizer->Add(progressGauge, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

  downloadInfoList =
      new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                     wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);
  downloadInfoList->InsertColumn(0, _("Fixture type"), wxLIST_FORMAT_LEFT, 260);
  downloadInfoList->InsertColumn(1, _("Selected GDTF"), wxLIST_FORMAT_LEFT,
                                 460);
  downloadInfoList->InsertColumn(2, _("Status"), wxLIST_FORMAT_LEFT, 170);
  downloadInfoList->InsertColumn(3, _("Progress"), wxLIST_FORMAT_LEFT, 220);
  downloadInfoList->InsertColumn(4, _("Details"), wxLIST_FORMAT_LEFT, 180);
  infoSizer->Add(downloadInfoList, 1, wxEXPAND | wxALL, 8);
  footerSummary = new wxStaticText(
      this, wxID_ANY, _("0 processed  |  0 downloaded  |  0 fallback"));
  footerSummary->SetForegroundColour(wxColour(100, 100, 100));
  infoSizer->Add(footerSummary, 0, wxLEFT | wxRIGHT, 8);
  bytesSummary = new wxStaticText(this, wxID_ANY, "0 B / ? B");
  bytesSummary->SetForegroundColour(wxColour(110, 110, 110));
  infoSizer->Add(bytesSummary, 0, wxLEFT | wxRIGHT | wxTOP, 8);
  wxBoxSizer *actionSizer = new wxBoxSizer(wxHORIZONTAL);
  cancelButton = new wxButton(this, wxID_CANCEL, _("Cancel"));
  ackButton = new wxButton(this, wxID_OK, _("OK"));
  ackButton->Disable();
  actionSizer->Add(cancelButton, 0, wxRIGHT, 8);
  actionSizer->Add(ackButton, 0);
  infoSizer->Add(actionSizer, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM,
                 8);
  SetSizer(infoSizer);
  if (parent) {
    CentreOnParent();
  } else {
    CentreOnScreen();
  }

  Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &event) {
    if (!finished) {
      RequestCancel();
      // Keep the stack-owned window alive until the workflow unwinds.
      if (event.CanVeto())
        event.Veto();
      return;
    }
    Acknowledge();
  });
  // Handle dialog-level commands as well as propagated button clicks, so the
  // native default handlers cannot dismiss an active workflow.
  Bind(
      wxEVT_BUTTON,
      [this](wxCommandEvent &) {
        if (finished)
          Acknowledge();
        else
          RequestCancel();
      },
      wxID_CANCEL);
  Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Acknowledge(); }, wxID_OK);
  // Do not let native Escape handling end the modal boundary during work.
  SetEscapeId(wxID_NONE);
  Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &event) {
    if (event.GetKeyCode() != WXK_ESCAPE) {
      event.Skip();
      return;
    }
    if (finished)
      Acknowledge();
    else
      RequestCancel();
  });
}

int GdtfDownloadQueueDialog::Run(std::function<void()> work) {
  wxCHECK_MSG(!running, wxID_CANCEL, "Queue dialog may only run once");
  running = true;
  std::exception_ptr error;
  // Start inside the one standard modal boundary. No second completion loop
  // or deferred worker callback can outlive the locally owned dialog.
  CallAfter([this, work = std::move(work), &error]() {
    try {
      work();
      Finish();
    } catch (...) {
      error = std::current_exception();
      finished = true;
      EndModal(wxID_CANCEL);
    }
  });
  const int result = ShowModal();
  if (error)
    std::rethrow_exception(error);
  return result;
}

void GdtfDownloadQueueDialog::RequestCancel() {
  if (finished)
    return;
  cancelRequested.store(true);
  cancelButton->Disable();
  SetPhase(_("Cancel requested. Finishing current transfer..."));
}

void GdtfDownloadQueueDialog::Acknowledge() {
  if (finished && IsModal())
    EndModal(wxID_OK);
}

void GdtfDownloadQueueDialog::Finish() {
  if (finished)
    return;
  finished = true;
  cancelButton->Disable();
  summaryText->SetLabel(
      wxString::Format(_("%s - queue finished"), summaryText->GetLabel()));
  RefreshFooterSummary();
  RefreshBytesSummary();
  UpdateProgressGauge();
  ackButton->Enable();
  if (autoClose)
    Acknowledge();
}

void GdtfDownloadQueueDialog::SetPhase(const wxString &text) {
  progressPhaseText->SetLabel(text);
}
void GdtfDownloadQueueDialog::SetSummary(const wxString &text) {
  summaryText->SetLabel(text);
}
void GdtfDownloadQueueDialog::SetProgress(int value) {
  progressGauge->SetValue(value);
}

void GdtfDownloadQueueDialog::AddRow(const std::string &type) {
  const long row = downloadInfoList->InsertItem(
      downloadInfoList->GetItemCount(), wxString::FromUTF8(type));
  downloadInfoList->SetItem(row, 1, "-");
  downloadInfoList->SetItem(row, 2, _("Pending"));
  downloadInfoList->SetItem(row, 3, "0 B / ? B");
  downloadInfoList->SetItem(row, 4, _("Waiting to match catalog entry"));
  downloadInfoList->SetItemTextColour(row,
                                      RowTextColor(DownloadRowState::Pending));
  rowByType[type] = row;
  rowStateByType[type] = DownloadRowState::Pending;
  rowProgressByType[type] = DownloadProgressStats{};
  RefreshFooterSummary();
  RefreshBytesSummary();
  UpdateProgressGauge();
}

wxString GdtfDownloadQueueDialog::FormatBytes(long long bytes) {
  if (bytes < 0)
    return "? B";
  static const char *kUnits[] = {"B", "KB", "MB", "GB", "TB"};
  double value = static_cast<double>(bytes);
  size_t unitIndex = 0;
  while (value >= 1024.0 && unitIndex < 4) {
    value /= 1024.0;
    ++unitIndex;
  }
  if (unitIndex == 0)
    return wxString::Format("%lld %s", bytes, kUnits[unitIndex]);
  return wxString::Format("%.1f %s", value, kUnits[unitIndex]);
}

wxString GdtfDownloadQueueDialog::FormatEta(long long seconds) {
  if (seconds < 0)
    return "ETA --:--";
  const long long mins = seconds / 60;
  const long long secs = seconds % 60;
  return wxString::Format("ETA %02lld:%02lld", mins, secs);
}

void GdtfDownloadQueueDialog::RefreshFooterSummary() {
  int downloaded = 0;
  int fallback = 0;
  int canceled = 0;
  int processed = 0;
  for (const auto &[typeKey, state] : rowStateByType) {
    (void)typeKey;
    if (state == DownloadRowState::Downloaded) {
      ++downloaded;
      ++processed;
    } else if (state == DownloadRowState::Fallback) {
      ++fallback;
      ++processed;
    } else if (state == DownloadRowState::Canceled) {
      ++canceled;
      ++processed;
    }
  }
  footerSummary->SetLabel(wxString::Format(
      _("%d/%zu processed  |  %d downloaded  |  "
        "%d fallback  |  %d canceled"),
      processed, rowStateByType.size(), downloaded, fallback, canceled));
}

void GdtfDownloadQueueDialog::RefreshBytesSummary() {
  long long downloaded = 0;
  long long knownTotal = 0;
  bool hasUnknownTotal = false;
  for (const auto &[typeKey, progress] : rowProgressByType) {
    (void)typeKey;
    downloaded += std::max<long long>(0, progress.downloadedBytes);
    if (progress.totalBytes > 0) {
      knownTotal += progress.totalBytes;
    } else {
      hasUnknownTotal = true;
    }
  }
  const wxString totalLabel =
      hasUnknownTotal ? wxString("? B") : FormatBytes(knownTotal);
  wxString label = FormatBytes(downloaded) + " / " + totalLabel;
  const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::steady_clock::now() - queueStartTime)
                           .count();
  if (!hasUnknownTotal && knownTotal > downloaded && elapsed > 0 &&
      downloaded > 0) {
    const double speed =
        static_cast<double>(downloaded) / static_cast<double>(elapsed);
    if (speed > 0.0) {
      const long long remainingSeconds = static_cast<long long>(
          static_cast<double>(knownTotal - downloaded) / speed);
      label += "  |  " + FormatEta(remainingSeconds);
    }
  }
  bytesSummary->SetLabel(label);
}

void GdtfDownloadQueueDialog::UpdateStatusRow(const std::string &typeKey,
                                              const wxString &selectedGdtf,
                                              const wxString &status,
                                              const wxString &progressText,
                                              const wxString &details,
                                              DownloadRowState state) {
  const auto rowIt = rowByType.find(typeKey);
  if (rowIt == rowByType.end())
    return;
  const long row = rowIt->second;
  downloadInfoList->SetItem(row, 1, selectedGdtf);
  downloadInfoList->SetItem(row, 2, status);
  downloadInfoList->SetItem(row, 3, progressText);
  downloadInfoList->SetItem(row, 4, details);
  downloadInfoList->SetItemTextColour(row, RowTextColor(state));
  rowStateByType[typeKey] = state;
  RefreshFooterSummary();
  RefreshBytesSummary();
}

void GdtfDownloadQueueDialog::UpdateProgressGauge() {
  long long downloaded = 0;
  long long knownTotal = 0;
  bool hasUnknownTotal = false;
  for (const auto &[typeKey, progress] : rowProgressByType) {
    (void)typeKey;
    downloaded += std::max<long long>(0, progress.downloadedBytes);
    if (progress.totalBytes > 0) {
      knownTotal += progress.totalBytes;
    } else {
      hasUnknownTotal = true;
    }
  }
  if (!hasUnknownTotal && knownTotal > 0) {
    const int value =
        static_cast<int>(std::clamp((static_cast<double>(downloaded) * 100.0) /
                                        static_cast<double>(knownTotal),
                                    0.0, 100.0));
    progressGauge->SetValue(value);
    return;
  }
  const int total = static_cast<int>(rowStateByType.size());
  int finished = 0;
  for (const auto &[typeKey, state] : rowStateByType) {
    (void)typeKey;
    if (state == DownloadRowState::Downloaded ||
        state == DownloadRowState::Fallback ||
        state == DownloadRowState::Canceled) {
      ++finished;
    }
  }
  const int safeTotal = std::max(1, total);
  progressGauge->SetValue((finished * 100) / safeTotal);
}
