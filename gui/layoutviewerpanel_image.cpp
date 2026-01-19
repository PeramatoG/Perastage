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

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>

#include <GL/gl.h>

#include "layoutimageutils.h"
#include "layouts/LayoutManager.h"

namespace {
constexpr int kMinFrameSize = 24;

void HashCombine(size_t &seed, size_t value) {
  seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
} // namespace

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
  if (!currentLayout.name.empty()) {
    layouts::LayoutManager::Get().UpdateLayoutImage(currentLayout.name, *image);
  }
  InvalidateRenderIfFrameChanged();
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::OnEditImage(wxCommandEvent &) {
  if (selectedElementType != SelectedElementType::Image)
    return;
  layouts::LayoutImageDefinition *image = GetSelectedImage();
  if (!image)
    return;

  auto result = PromptForLayoutImage(this, "Selecciona una imagen");
  if (!result)
    return;

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
  renderDirty = true;
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::OnDeleteImage(wxCommandEvent &) {
  if (selectedElementType != SelectedElementType::Image)
    return;
  const layouts::LayoutImageDefinition *image = GetSelectedImage();
  if (!image)
    return;
  const int imageId = image->id;
  if (!currentLayout.name.empty()) {
    if (layouts::LayoutManager::Get().RemoveLayoutImage(currentLayout.name,
                                                        imageId)) {
      auto &images = currentLayout.imageViews;
      images.erase(std::remove_if(images.begin(), images.end(),
                                  [imageId](const auto &entry) {
                                    return entry.id == imageId;
                                  }),
                   images.end());
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
  } else {
    glColor4ub(160, 160, 160, 255);
    glLineWidth(1.0f);
  }
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

  if (image.id == activeImageId)
    DrawSelectionHandles(frameRect);
}

size_t LayoutViewerPanel::HashImageContent(
    const layouts::LayoutImageDefinition &image) const {
  size_t seed = std::hash<std::string>{}(image.imagePath);
  std::error_code ec;
  if (!image.imagePath.empty()) {
    const std::filesystem::path path(image.imagePath);
    if (std::filesystem::exists(path, ec)) {
      const auto fileSize = std::filesystem::file_size(path, ec);
      if (!ec)
        HashCombine(seed, std::hash<uintmax_t>{}(fileSize));
      const auto writeTime = std::filesystem::last_write_time(path, ec);
      if (!ec) {
        const auto stamp = writeTime.time_since_epoch().count();
        HashCombine(seed,
                    std::hash<decltype(stamp)>{}(stamp));
      }
    }
  }
  HashCombine(seed, std::hash<float>{}(image.aspectRatio));
  return seed;
}
