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
#include "layoutimageutils.h"

#include <filesystem>
#include <sstream>

#include <wx/filedlg.h>
#include <wx/msgdlg.h>

namespace {
constexpr size_t kMaxLayoutImageBytes = 5 * 1024 * 1024;

wxString LayoutImageWildcard() {
  return "Image files (*.png;*.jpg;*.jpeg;*.bmp;*.gif)|*.png;*.jpg;*.jpeg;*.bmp;*.gif";
}

bool LoadLayoutImageFile(const wxString &path, wxImage &image,
                         wxString &error) {
  if (path.empty()) {
    error = "No image path selected.";
    return false;
  }
  std::error_code ec;
  const std::filesystem::path filePath(path.ToStdString());
  if (!std::filesystem::exists(filePath, ec)) {
    error = "The selected image file does not exist.";
    return false;
  }
  const auto fileSize = std::filesystem::file_size(filePath, ec);
  if (ec) {
    error = "Unable to read the selected image file.";
    return false;
  }
  if (fileSize > kMaxLayoutImageBytes) {
    std::ostringstream stream;
    stream << "The selected image is too large. Please choose an image under "
           << (kMaxLayoutImageBytes / (1024 * 1024)) << " MB.";
    error = stream.str();
    return false;
  }
  if (!wxImage::CanRead(path)) {
    error =
        "Unsupported image format. Please select a PNG, JPG, BMP, or GIF file.";
    return false;
  }
  if (!image.LoadFile(path) || !image.IsOk()) {
    error = "Failed to load the selected image.";
    return false;
  }
  return true;
}
} // namespace

std::optional<LayoutImageLoadResult> PromptForLayoutImage(wxWindow *parent,
                                                          const wxString &title) {
  wxFileDialog dialog(parent, title, wxEmptyString, wxEmptyString,
                      LayoutImageWildcard(),
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (dialog.ShowModal() != wxID_OK)
    return std::nullopt;

  wxImage image;
  wxString error;
  if (!LoadLayoutImageFile(dialog.GetPath(), image, error)) {
    wxMessageBox(error, "Invalid image", wxOK | wxICON_WARNING, parent);
    return std::nullopt;
  }

  LayoutImageLoadResult result;
  result.path = dialog.GetPath();
  result.image = image;
  if (image.GetHeight() > 0) {
    result.aspectRatio = static_cast<float>(image.GetWidth()) /
                         static_cast<float>(image.GetHeight());
  }
  return result;
}
