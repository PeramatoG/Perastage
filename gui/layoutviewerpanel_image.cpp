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
#include "layoutviewerpanel.h"
#include "filesystem_path_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <system_error>

// Include GLEW or other OpenGL loader first if present
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#ifdef __APPLE__
#  include <OpenGL/gl.h>
#  include <OpenGL/glu.h>
#else
#  include <GL/gl.h>
#  include <GL/glu.h>
#endif

#include "layoutimageutils.h"
#include "LayoutManager.h"
#include "guiconfigservices.h"
#include "configmanager.h"

namespace {
constexpr int kMinFrameSize = 24;

struct ImageSourceFileState {
  std::uintmax_t size = 0;
  std::int64_t writeTime = 0;
  bool valid = false;
};

// Mixes a value into an existing hash seed for layout image cache keys.
void HashCombine(size_t &seed, size_t value) {
  seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// Reads the on-disk size and mtime used to validate decoded source image cache entries.
ImageSourceFileState GetImageSourceFileState(const std::string &imagePath) {
  ImageSourceFileState state;
  if (imagePath.empty())
    return state;

  std::error_code ec;
  try {
    const std::filesystem::path path = PathUtils::PathFromUtf8(imagePath);
    if (!std::filesystem::exists(path, ec) || ec)
      return state;

    const auto fileSize = std::filesystem::file_size(path, ec);
    if (ec)
      return state;

    const auto writeTime = std::filesystem::last_write_time(path, ec);
    if (ec)
      return state;

    state.size = fileSize;
    state.writeTime = static_cast<std::int64_t>(
        writeTime.time_since_epoch().count());
    state.valid = true;
  } catch (const std::exception &) {
    state.valid = false;
  }
  return state;
}
} // namespace

// Returns the selected image definition, defaulting to the first image when needed.
layouts::LayoutImageDefinition *LayoutViewerPanel::GetSelectedImage() {
  if (currentLayout.imageViews.empty())
    return nullptr;
  if (selectedElementType == SelectedElementType::Image &&
      selectedElementId >= 0) {
    for (auto &image : currentLayout.imageViews) {
      if (image.id == selectedElementId)
        return &image;
    }
  }
  selectedElementType = SelectedElementType::Image;
  selectedElementId = currentLayout.imageViews.front().id;
  return &currentLayout.imageViews.front();
}

// Returns the selected image definition without modifying the current selection.
const layouts::LayoutImageDefinition *LayoutViewerPanel::GetSelectedImage()
    const {
  if (currentLayout.imageViews.empty())
    return nullptr;
  if (selectedElementType == SelectedElementType::Image &&
      selectedElementId >= 0) {
    for (const auto &image : currentLayout.imageViews) {
      if (image.id == selectedElementId)
        return &image;
    }
  }
  if (!currentLayout.imageViews.empty())
    return &currentLayout.imageViews.front();
  return nullptr;
}

// Finds an image frame by identifier and copies it into the output parameter.
bool LayoutViewerPanel::GetImageFrameById(
    int imageId, layouts::Layout2DViewFrame &frame) const {
  if (imageId <= 0)
    return false;
  for (const auto &image : currentLayout.imageViews) {
    if (image.id == imageId) {
      frame = image.frame;
      return true;
    }
  }
  return false;
}

// Applies a frame update to the selected image and schedules texture refresh work.
void LayoutViewerPanel::UpdateImageFrame(const layouts::Layout2DViewFrame &frame,
                                         bool updatePosition) {
  layouts::LayoutImageDefinition *image = GetSelectedImage();
  if (!image)
    return;
  image->frame.width = frame.width;
  image->frame.height = frame.height;
  if (updatePosition) {
    image->frame.x = frame.x;
    image->frame.y = frame.y;
  }
  if (updatePosition) {
    pendingFrameCommit_ = true;
    Refresh();
    return;
  }
  if (!currentLayout.name.empty()) {
    layouts::LayoutManager::Get().UpdateLayoutImage(currentLayout.name, *image);
  }
  InvalidateRenderIfFrameChanged(false);
  if (NeedsRenderRebuild()) {
    RequestRenderRebuild();
  }
  Refresh();
}

// Opens the image picker and updates the selected layout image from the chosen file.
void LayoutViewerPanel::OnEditImage(wxCommandEvent &) {
  if (selectedElementType != SelectedElementType::Image)
    return;
  layouts::LayoutImageDefinition *image = GetSelectedImage();
  if (!image)
    return;

  auto result = PromptForLayoutImage(this, "Selecciona una imagen");
  if (!result)
    return;
  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  cfg.PushUndoState("edit layout image");

  wxScopedCharBuffer pathBuf = result->path.ToUTF8();
  image->imagePath = pathBuf.data() ? pathBuf.data() : "";
  image->aspectRatio = result->aspectRatio;

  if (image->aspectRatio > 0.0f) {
    if (image->frame.width > 0) {
      image->frame.height = std::max(
          kMinFrameSize,
          static_cast<int>(std::lround(image->frame.width /
                                       image->aspectRatio)));
    } else if (image->frame.height > 0) {
      image->frame.width = std::max(
          kMinFrameSize,
          static_cast<int>(std::lround(image->frame.height *
                                       image->aspectRatio)));
    } else {
      image->frame.width = kMinFrameSize;
      image->frame.height = std::max(
          kMinFrameSize,
          static_cast<int>(std::lround(image->frame.width /
                                       image->aspectRatio)));
    }
  }

  if (!currentLayout.name.empty()) {
    layouts::LayoutManager::Get().UpdateLayoutImage(currentLayout.name, *image);
  }
  ImageCache &cache = GetImageCache(image->id);
  cache.renderDirty = true;
  RequestRenderRebuild();
  Refresh();
}

// Removes the selected image element from the layout and releases its cache entry.
void LayoutViewerPanel::OnDeleteImage(wxCommandEvent &) {
  if (selectedElementType != SelectedElementType::Image)
    return;
  const layouts::LayoutImageDefinition *image = GetSelectedImage();
  if (!image)
    return;
  const int imageId = image->id;
  if (!currentLayout.name.empty()) {
    auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
    cfg.PushUndoState("delete layout image");
    if (layouts::LayoutManager::Get().RemoveLayoutImage(currentLayout.name,
                                                        imageId)) {
      auto &images = currentLayout.imageViews;
      images.erase(std::remove_if(images.begin(), images.end(),
                                  [imageId](const auto &entry) {
                                    return entry.id == imageId;
                                  }),
                   images.end());
      InvalidateSelectionIndexCache();
      if (selectedElementId == imageId) {
        if (!currentLayout.view2dViews.empty()) {
          selectedElementType = SelectedElementType::View2D;
          selectedElementId = currentLayout.view2dViews.front().id;
        } else if (!currentLayout.legendViews.empty()) {
          selectedElementType = SelectedElementType::Legend;
          selectedElementId = currentLayout.legendViews.front().id;
        } else if (!currentLayout.eventTables.empty()) {
          selectedElementType = SelectedElementType::EventTable;
          selectedElementId = currentLayout.eventTables.front().id;
        } else if (!currentLayout.textViews.empty()) {
          selectedElementType = SelectedElementType::Text;
          selectedElementId = currentLayout.textViews.front().id;
        } else if (!images.empty()) {
          selectedElementType = SelectedElementType::Image;
          selectedElementId = images.front().id;
        } else {
          selectedElementType = SelectedElementType::None;
          selectedElementId = -1;
        }
      }
    }
  }
  auto cacheIt = imageCaches_.find(imageId);
  if (cacheIt != imageCaches_.end()) {
    ClearCachedTexture(cacheIt->second);
    imageCaches_.erase(cacheIt);
  }
  Refresh();
}

// Draws a layout image element using its cached GL texture or a placeholder fill.
void LayoutViewerPanel::DrawImageElement(
    const layouts::LayoutImageDefinition &image, int activeImageId) {
  ImageCache &cache = GetImageCache(image.id);
  wxRect frameRect;
  if (!GetFrameRect(image.frame, frameRect))
    return;
  const int frameRight = frameRect.GetLeft() + frameRect.GetWidth();
  const int frameBottom = frameRect.GetTop() + frameRect.GetHeight();

  const wxSize renderSize = GetFrameSizeForZoom(image.frame, cache.renderZoom);
  if (cache.texture != 0 && renderSize.GetWidth() > 0 &&
      renderSize.GetHeight() > 0 && cache.textureSize == renderSize) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, cache.texture);
    glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(static_cast<float>(frameRect.GetLeft()),
               static_cast<float>(frameRect.GetTop()));
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(static_cast<float>(frameRight),
               static_cast<float>(frameRect.GetTop()));
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(static_cast<float>(frameRight),
               static_cast<float>(frameBottom));
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(static_cast<float>(frameRect.GetLeft()),
               static_cast<float>(frameRect.GetBottom()));
    glEnd();
    glDisable(GL_TEXTURE_2D);
  } else {
    glColor4ub(230, 230, 230, 255);
    glBegin(GL_QUADS);
    glVertex2f(static_cast<float>(frameRect.GetLeft()),
               static_cast<float>(frameRect.GetTop()));
    glVertex2f(static_cast<float>(frameRect.GetRight()),
               static_cast<float>(frameRect.GetTop()));
    glVertex2f(static_cast<float>(frameRight),
               static_cast<float>(frameBottom));
    glVertex2f(static_cast<float>(frameRect.GetLeft()),
               static_cast<float>(frameRect.GetBottom()));
    glEnd();
  }

  if (image.id == activeImageId) {
    glColor4ub(60, 160, 240, 255);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(static_cast<float>(frameRect.GetLeft()),
               static_cast<float>(frameRect.GetTop()));
    glVertex2f(static_cast<float>(frameRight),
               static_cast<float>(frameRect.GetTop()));
    glVertex2f(static_cast<float>(frameRight),
               static_cast<float>(frameBottom));
    glVertex2f(static_cast<float>(frameRect.GetLeft()),
               static_cast<float>(frameRect.GetBottom()));
    glEnd();
    DrawSelectionHandles(frameRect);
  }
}

// Clears the decoded source image while leaving GL texture resources untouched.
void LayoutViewerPanel::ClearCachedImageSource(ImageCache &cache) {
  cache.sourceImage = wxImage();
  cache.sourceImagePath.clear();
  cache.sourceFileSize = 0;
  cache.sourceWriteTime = 0;
  cache.hasSourceImage = false;
  cache.hasSourceFileState = false;
}

// Loads or reuses the decoded source image when the path and file state are unchanged.
bool LayoutViewerPanel::EnsureCachedImageSource(
    const layouts::LayoutImageDefinition &image, ImageCache &cache) {
  const ImageSourceFileState fileState =
      GetImageSourceFileState(image.imagePath);
  const bool sourceCurrent =
      cache.hasSourceImage && cache.sourceImage.IsOk() &&
      cache.sourceImagePath == image.imagePath && fileState.valid &&
      cache.hasSourceFileState && cache.sourceFileSize == fileState.size &&
      cache.sourceWriteTime == fileState.writeTime;
  if (sourceCurrent)
    return cache.sourceImage.GetWidth() > 0 && cache.sourceImage.GetHeight() > 0;

  ClearCachedImageSource(cache);
  if (image.imagePath.empty() || !fileState.valid)
    return false;

  wxImage source;
  if (!source.LoadFile(wxString::FromUTF8(image.imagePath)))
    return false;
  if (!source.IsOk() || source.GetWidth() <= 0 || source.GetHeight() <= 0)
    return false;

  cache.sourceImage = source;
  cache.sourceImagePath = image.imagePath;
  cache.sourceFileSize = fileState.size;
  cache.sourceWriteTime = fileState.writeTime;
  cache.hasSourceImage = true;
  cache.hasSourceFileState = true;
  return true;
}

// Computes the cache key for a layout image, including path and file metadata.
size_t LayoutViewerPanel::HashImageContent(
    const layouts::LayoutImageDefinition &image) const {
  size_t seed = std::hash<std::string>{}(image.imagePath);
  const ImageSourceFileState fileState =
      GetImageSourceFileState(image.imagePath);
  if (fileState.valid) {
    HashCombine(seed, std::hash<std::uintmax_t>{}(fileState.size));
    HashCombine(seed, std::hash<std::int64_t>{}(fileState.writeTime));
  }
  HashCombine(seed, std::hash<float>{}(image.aspectRatio));
  return seed;
}
