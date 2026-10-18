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
#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <wx/dialog.h>

class wxButton;
class wxGauge;
class wxListCtrl;
class wxStaticText;

enum class DownloadRowState {
  Pending,
  Downloading,
  Downloaded,
  Fallback,
  Canceled
};
struct DownloadProgressStats {
  long long downloadedBytes = 0;
  long long totalBytes = -1;
};

// Owns the queue's presentation for the entire synchronous import boundary.
class GdtfDownloadQueueDialog : public wxDialog {
public:
  GdtfDownloadQueueDialog(wxWindow *parent, bool autoClose);
  int Run(std::function<void()> work);
  void AddRow(const std::string &type);
  bool IsFinished() const { return finished; }
  bool IsCancellationRequested() const { return cancelRequested.load(); }
  void SetPhase(const wxString &text);
  void SetSummary(const wxString &text);
  void SetProgress(int value);
  DownloadProgressStats &Progress(const std::string &type) {
    return rowProgressByType.at(type);
  }
  DownloadRowState State(const std::string &type) const {
    return rowStateByType.at(type);
  }
  static wxString FormatBytes(long long bytes);
  void UpdateStatusRow(const std::string &typeKey, const wxString &selectedGdtf,
                       const wxString &status, const wxString &progressText,
                       const wxString &details, DownloadRowState state);
  void UpdateProgressGauge();

private:
  void Finish();
  static wxString FormatEta(long long seconds);
  void RefreshFooterSummary();
  void RefreshBytesSummary();
  void RequestCancel();
  void Acknowledge();
  const bool autoClose;
  bool finished = false;
  bool running = false;
  std::atomic<bool> cancelRequested{false};
  wxStaticText *summaryText = nullptr;
  wxStaticText *progressPhaseText = nullptr;
  wxGauge *progressGauge = nullptr;
  wxListCtrl *downloadInfoList = nullptr;
  wxStaticText *footerSummary = nullptr;
  wxStaticText *bytesSummary = nullptr;
  wxButton *cancelButton = nullptr;
  wxButton *ackButton = nullptr;
  std::unordered_map<std::string, long> rowByType;
  std::unordered_map<std::string, DownloadRowState> rowStateByType;
  std::unordered_map<std::string, DownloadProgressStats> rowProgressByType;
  const std::chrono::steady_clock::time_point queueStartTime =
      std::chrono::steady_clock::now();
};
