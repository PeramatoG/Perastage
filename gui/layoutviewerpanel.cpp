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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <future>
#include <list>
#include <filesystem>
#include <new>
#include <unordered_map>
#include <vector>
#include <wx/weakref.h>
#include <wx/window.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#endif

#include <GL/glew.h>
#include "glew_init_utils.h"
// Include GLEW or other OpenGL loader first if present
#ifdef __APPLE__
#  define GL_SILENCE_DEPRECATION
#  include <OpenGL/gl.h>
#  include <OpenGL/glu.h>
#else
#  include <GL/gl.h>
#  include <GL/glu.h>
#endif

#include "layoutviewerpanel.h"
#include "gl_state_guard.h"
#include <wx/debug.h>
#include <wx/log.h>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif
#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY 0x88B9
#endif

#include "configmanager.h"
#include "editable_focus_utils.h"
#include "guiconfigservices.h"
#include "legendsymbolcapture.h"
#include "LayoutManager.h"
#include "logger.h"
#include "mainwindow.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dstate.h"
#include "ui_render_size.h"
#include "ui_feature_flags.h"

namespace {
constexpr double kMinZoom = 0.25;
constexpr double kMaxZoom = 10.0;
constexpr double kZoomStep = 1.1;
constexpr int kZoomCacheStepsPerLevel = 2;
constexpr int kFitMarginPx = 40;
constexpr int kHandleSizePx = 10;
constexpr int kHandleHalfPx = kHandleSizePx / 2;
constexpr int kHandleHoverPadPx = 6;
constexpr int kMinFrameSize = 24;
constexpr int kLayoutGridStep = 5;
constexpr int kMaxRenderDimension = 8192;
constexpr size_t kMaxRenderPixels =
    static_cast<size_t>(kMaxRenderDimension) * kMaxRenderDimension;
constexpr size_t kMaxRenderBytes = 64 * 1024 * 1024;
constexpr int kEditMenuId = wxID_HIGHEST + 490;
constexpr int kDeleteMenuId = wxID_HIGHEST + 491;
constexpr int kDeleteLegendMenuId = wxID_HIGHEST + 492;
constexpr int kEditLegendMenuId = wxID_HIGHEST + 505;
constexpr int kEditEventTableMenuId = wxID_HIGHEST + 493;
constexpr int kDeleteEventTableMenuId = wxID_HIGHEST + 494;
constexpr int kEditTextMenuId = wxID_HIGHEST + 495;
constexpr int kDeleteTextMenuId = wxID_HIGHEST + 496;
constexpr int kEditImageMenuId = wxID_HIGHEST + 497;
constexpr int kDeleteImageMenuId = wxID_HIGHEST + 498;
constexpr int kBringToFrontMenuId = wxID_HIGHEST + 499;
constexpr int kSendToBackMenuId = wxID_HIGHEST + 500;
constexpr int kLoadingTimerId = wxID_HIGHEST + 501;
constexpr int kRenderDelayTimerId = wxID_HIGHEST + 502;
constexpr int kToggleTextFrameMenuId = wxID_HIGHEST + 503;
constexpr int kToggleTextTransparentBackgroundMenuId = wxID_HIGHEST + 504;
constexpr int kToggleViewFrameMenuId = wxID_HIGHEST + 506;
constexpr int kLoadingOverlayDelayMs = 150;
constexpr size_t kImageBitmapLruCapacity = 64;
constexpr size_t kImageBitmapLruMaxBytes = 128u * 1024u * 1024u;

struct ImageBitmapCacheKey {
  std::string imagePath;
  int targetWidth = 0;
  int targetHeight = 0;
  std::filesystem::file_time_type fileMtime{};

  bool operator==(const ImageBitmapCacheKey &other) const {
    return imagePath == other.imagePath && targetWidth == other.targetWidth &&
           targetHeight == other.targetHeight && fileMtime == other.fileMtime;
  }
};

struct ImageBitmapCacheKeyHasher {
  size_t operator()(const ImageBitmapCacheKey &key) const {
    const auto mtimeTicks =
        static_cast<long long>(key.fileMtime.time_since_epoch().count());
    size_t seed = std::hash<std::string>{}(key.imagePath);
    seed ^= std::hash<int>{}(key.targetWidth) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= std::hash<int>{}(key.targetHeight) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= std::hash<long long>{}(mtimeTicks) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
  }
};

struct ImageBitmapCacheEntry {
  ImageBitmapCacheKey key;
  wxImage bitmap;
  size_t byteSize = 0;
};

// Returns the image file modification time and reports false when it cannot be read.
bool TryGetImageFileMtime(const std::string &imagePath,
                          std::filesystem::file_time_type &outMtime) {
  std::error_code ec;
  outMtime = std::filesystem::last_write_time(std::filesystem::path(imagePath), ec);
  return !ec;
}


// Estimates the RGBA memory footprint used by a scaled image cache entry.
size_t EstimateImageCacheEntryBytes(const wxImage &image) {
  const int width = image.GetWidth();
  const int height = image.GetHeight();
  if (width <= 0 || height <= 0) {
    return 0;
  }
  return static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
}

// Returns a mirrored and alpha-ready scaled bitmap from a process-local LRU cache.
const wxImage *GetOrCreateScaledImageBitmap(const std::string &imagePath,
                                            int targetWidth, int targetHeight,
                                            std::filesystem::file_time_type fileMtime) {
  using CacheList = std::list<ImageBitmapCacheEntry>;
  static CacheList cacheEntries;
  static std::unordered_map<ImageBitmapCacheKey, CacheList::iterator, ImageBitmapCacheKeyHasher>
      cacheIndex;
  static size_t cacheBytes = 0;

  const ImageBitmapCacheKey key{imagePath, targetWidth, targetHeight, fileMtime};
  auto it = cacheIndex.find(key);
  if (it != cacheIndex.end()) {
    cacheEntries.splice(cacheEntries.begin(), cacheEntries, it->second);
    return &it->second->bitmap;
  }

  wxImage sourceBitmap;
  if (!sourceBitmap.LoadFile(wxString::FromUTF8(imagePath)) ||
      sourceBitmap.GetWidth() <= 0 || sourceBitmap.GetHeight() <= 0) {
    return nullptr;
  }

  wxImage scaled = sourceBitmap.Scale(targetWidth, targetHeight, wxIMAGE_QUALITY_HIGH);
  if (!scaled.IsOk()) {
    return nullptr;
  }
  scaled = scaled.Mirror(false);
  if (!scaled.HasAlpha()) {
    scaled.InitAlpha();
  }

  const size_t scaledBytes = EstimateImageCacheEntryBytes(scaled);
  cacheEntries.push_front({key, scaled, scaledBytes});
  cacheIndex[key] = cacheEntries.begin();
  cacheBytes += scaledBytes;
  while (cacheEntries.size() > kImageBitmapLruCapacity ||
         cacheBytes > kImageBitmapLruMaxBytes) {
    auto tail = std::prev(cacheEntries.end());
    cacheBytes -= tail->byteSize;
    cacheIndex.erase(tail->key);
    cacheEntries.pop_back();
  }
  return &cacheEntries.front().bitmap;
}

void ValidateGlStateAfterRender(const char *stage, int expectedWidth,
                                int expectedHeight) {
  GLint framebuffer = 0;
  GLint viewport[4] = {0, 0, 0, 0};
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
  glGetIntegerv(GL_VIEWPORT, viewport);
  const bool validFramebuffer = framebuffer == 0;
  const bool validViewport =
      viewport[0] == 0 && viewport[1] == 0 && viewport[2] == expectedWidth &&
      viewport[3] == expectedHeight;
  if (!validFramebuffer || !validViewport) {
    wxLogTrace("layoutviewer_gl_state",
               "%s left unexpected GL state (fbo=%d viewport=%d,%d,%d,%d "
               "expected=0,0,%d,%d).",
               stage, framebuffer, viewport[0], viewport[1], viewport[2],
               viewport[3], expectedWidth, expectedHeight);
  }
  if (!validFramebuffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    wxASSERT_MSG(false,
                 "Unexpected non-default framebuffer after layout render.");
  }
  if (!validViewport) {
    // The layout draw path can temporarily adjust the viewport for sub-elements.
    // Restore a known onscreen viewport before presenting.
    glViewport(0, 0, expectedWidth, expectedHeight);
  }
}

double GetMaxZoomForFrame(const layouts::Layout2DViewFrame &frame) {
  if (frame.width <= 0 || frame.height <= 0)
    return kMaxZoom;

  const double width = static_cast<double>(frame.width);
  const double height = static_cast<double>(frame.height);
  const double byDimension =
      std::min(static_cast<double>(kMaxRenderDimension) / width,
               static_cast<double>(kMaxRenderDimension) / height);
  const double sourcePixels = width * height;
  if (sourcePixels <= 0.0)
    return std::clamp(byDimension, kMinZoom, kMaxZoom);

  const double maxPixelsByArea =
      static_cast<double>(kMaxRenderBytes / 4) / sourcePixels;
  if (maxPixelsByArea <= 0.0)
    return std::clamp(byDimension, kMinZoom, kMaxZoom);

  const double byPixelBudget = std::sqrt(maxPixelsByArea);
  const double maxZoom = std::min(byDimension, byPixelBudget);
  return std::clamp(maxZoom, kMinZoom, kMaxZoom);
}

double GetLayoutSafeMaxZoom(const layouts::LayoutDefinition &layout) {
  double maxZoom = kMaxZoom;
  auto clampFromFrames = [&maxZoom](const auto &collection) {
    for (const auto &entry : collection) {
      maxZoom = std::min(maxZoom, GetMaxZoomForFrame(entry.frame));
    }
  };

  clampFromFrames(layout.view2dViews);
  clampFromFrames(layout.legendViews);
  clampFromFrames(layout.eventTables);
  clampFromFrames(layout.textViews);
  clampFromFrames(layout.imageViews);
  return std::clamp(maxZoom, kMinZoom, kMaxZoom);
}

bool AreEqual(const layouts::Layout2DViewFrame &lhs,
              const layouts::Layout2DViewFrame &rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.width == rhs.width &&
         lhs.height == rhs.height;
}

bool AreEqual(const layouts::Layout2DViewCameraState &lhs,
              const layouts::Layout2DViewCameraState &rhs) {
  return lhs.offsetPixelsX == rhs.offsetPixelsX &&
         lhs.offsetPixelsY == rhs.offsetPixelsY && lhs.zoom == rhs.zoom &&
         lhs.viewportWidth == rhs.viewportWidth &&
         lhs.viewportHeight == rhs.viewportHeight && lhs.view == rhs.view;
}

bool AreEqual(const layouts::Layout2DViewRenderOptions &lhs,
              const layouts::Layout2DViewRenderOptions &rhs) {
  return lhs.renderMode == rhs.renderMode && lhs.darkMode == rhs.darkMode &&
         lhs.forceBottomViewForTopFixtures == rhs.forceBottomViewForTopFixtures &&
         lhs.showGrid == rhs.showGrid && lhs.gridStyle == rhs.gridStyle &&
         lhs.gridColorR == rhs.gridColorR && lhs.gridColorG == rhs.gridColorG &&
         lhs.gridColorB == rhs.gridColorB &&
         lhs.gridDrawAbove == rhs.gridDrawAbove &&
         lhs.showRuler == rhs.showRuler &&
         lhs.rulerColorR == rhs.rulerColorR &&
         lhs.rulerColorG == rhs.rulerColorG &&
         lhs.rulerColorB == rhs.rulerColorB &&
         lhs.showLabelName == rhs.showLabelName &&
         lhs.showLabelId == rhs.showLabelId &&
         lhs.showLabelDmx == rhs.showLabelDmx &&
         lhs.labelFontSizeName == rhs.labelFontSizeName &&
         lhs.labelFontSizeId == rhs.labelFontSizeId &&
         lhs.labelFontSizeDmx == rhs.labelFontSizeDmx &&
         lhs.labelOffsetDistance == rhs.labelOffsetDistance &&
         lhs.labelOffsetAngle == rhs.labelOffsetAngle;
}

bool AreEqual(const layouts::Layout2DViewLayers &lhs,
              const layouts::Layout2DViewLayers &rhs) {
  return lhs.hiddenLayers == rhs.hiddenLayers &&
         lhs.hiddenFixtureTypes == rhs.hiddenFixtureTypes;
}

bool AreEqual(const layouts::Layout2DViewDefinition &lhs,
              const layouts::Layout2DViewDefinition &rhs) {
  return lhs.id == rhs.id && lhs.zIndex == rhs.zIndex &&
         AreEqual(lhs.frame, rhs.frame) && AreEqual(lhs.camera, rhs.camera) &&
         AreEqual(lhs.renderOptions, rhs.renderOptions) &&
         lhs.drawFrame == rhs.drawFrame &&
         AreEqual(lhs.layers, rhs.layers);
}

bool AreEqual(const layouts::LayoutLegendDefinition &lhs,
              const layouts::LayoutLegendDefinition &rhs) {
  return lhs.id == rhs.id && lhs.zIndex == rhs.zIndex &&
         AreEqual(lhs.frame, rhs.frame);
}

bool AreEqual(const layouts::LayoutEventTableDefinition &lhs,
              const layouts::LayoutEventTableDefinition &rhs) {
  return lhs.id == rhs.id && lhs.zIndex == rhs.zIndex &&
         AreEqual(lhs.frame, rhs.frame) && lhs.fields == rhs.fields;
}

bool AreEqual(const layouts::LayoutTextDefinition &lhs,
              const layouts::LayoutTextDefinition &rhs) {
  return lhs.id == rhs.id && lhs.zIndex == rhs.zIndex &&
         AreEqual(lhs.frame, rhs.frame) && lhs.text == rhs.text &&
         lhs.richText == rhs.richText &&
         lhs.solidBackground == rhs.solidBackground &&
         lhs.drawFrame == rhs.drawFrame;
}

bool AreEqual(const layouts::LayoutImageDefinition &lhs,
              const layouts::LayoutImageDefinition &rhs) {
  return lhs.id == rhs.id && lhs.zIndex == rhs.zIndex &&
         AreEqual(lhs.frame, rhs.frame) && lhs.imagePath == rhs.imagePath &&
         lhs.aspectRatio == rhs.aspectRatio;
}

template <typename T>
bool AreEqual(const std::vector<T> &lhs, const std::vector<T> &rhs) {
  if (lhs.size() != rhs.size())
    return false;

  for (size_t i = 0; i < lhs.size(); ++i) {
    if (!AreEqual(lhs[i], rhs[i]))
      return false;
  }
  return true;
}

wxSize GetLogicalClientSize(const wxWindow *window) {
  if (window == nullptr) {
    return wxSize(0, 0);
  }
  return window->GetClientSize();
}

wxPoint GetLogicalMousePosition(const wxMouseEvent &event) {
  return event.GetPosition();
}

wxPoint ToFramebufferPoint(wxWindow *window, const wxPoint &logicalPoint) {
  if (window == nullptr) {
    return wxPoint(0, 0);
  }
  const double contentScale =
      static_cast<double>(window->GetContentScaleFactor());
  if (!std::isfinite(contentScale) || contentScale <= 0.0) {
    return wxPoint(0, 0);
  }
  return wxPoint(static_cast<int>(std::lround(logicalPoint.x * contentScale)),
                 static_cast<int>(std::lround(logicalPoint.y * contentScale)));
}

bool IsSameRenderableLayout(const layouts::LayoutDefinition &lhs,
                            const layouts::LayoutDefinition &rhs) {
  return lhs.pageSetup.pageSize == rhs.pageSetup.pageSize &&
         lhs.pageSetup.landscape == rhs.pageSetup.landscape &&
         AreEqual(lhs.view2dViews, rhs.view2dViews) &&
         AreEqual(lhs.legendViews, rhs.legendViews) &&
         AreEqual(lhs.eventTables, rhs.eventTables) &&
         AreEqual(lhs.textViews, rhs.textViews) &&
         AreEqual(lhs.imageViews, rhs.imageViews);
}

unsigned int *gActivePixelUnpackPbo = nullptr;
size_t *gActivePixelUnpackPboBytes = nullptr;

class ScopedActivePixelUnpackPbo {
public:
  ScopedActivePixelUnpackPbo(unsigned int &pbo, size_t &capacity)
      : previousPbo_(gActivePixelUnpackPbo),
        previousCapacity_(gActivePixelUnpackPboBytes) {
    gActivePixelUnpackPbo = &pbo;
    gActivePixelUnpackPboBytes = &capacity;
  }

  ~ScopedActivePixelUnpackPbo() {
    gActivePixelUnpackPbo = previousPbo_;
    gActivePixelUnpackPboBytes = previousCapacity_;
  }

private:
  unsigned int *previousPbo_;
  size_t *previousCapacity_;
};

bool TryAllocatePixelBuffer(std::vector<unsigned char> &pixels, int width,
                            int height, const char *context) {
  if (width <= 0 || height <= 0)
    return false;
  const size_t totalPixels =
      static_cast<size_t>(width) * static_cast<size_t>(height);
  const size_t totalBytes = totalPixels * 4;
  if (totalBytes > kMaxRenderBytes) {
    Logger::Instance().Log(
        std::string("LayoutViewerPanel: ") + context +
        " render buffer exceeds kMaxRenderBytes (" +
        std::to_string(totalBytes) + " > " + std::to_string(kMaxRenderBytes) +
        ") for " + std::to_string(width) + "x" + std::to_string(height) +
        ".");
    return false;
  }
  if (totalPixels > kMaxRenderPixels) {
    Logger::Instance().Log(
        std::string("LayoutViewerPanel: ") + context +
        " render buffer too large (" + std::to_string(width) + "x" +
        std::to_string(height) + ").");
    return false;
  }
  try {
    pixels.resize(totalBytes);
  } catch (const std::bad_alloc &) {
    Logger::Instance().Log(
        std::string("LayoutViewerPanel: ") + context +
        " render buffer allocation failed.");
    return false;
  }
  return true;
}

bool IsPixelUnpackPboSupported() {
  static int cachedSupport = -1;
  if (cachedSupport >= 0)
    return cachedSupport == 1;

  const GLubyte *versionData = glGetString(GL_VERSION);
  if (!versionData) {
    cachedSupport = 0;
    return false;
  }

  int major = 0;
  int minor = 0;
  if (std::sscanf(reinterpret_cast<const char *>(versionData), "%d.%d", &major,
                  &minor) == 2) {
    if (major > 2 || (major == 2 && minor >= 1)) {
      cachedSupport = 1;
      return true;
    }
  }

  const GLubyte *extensionsData = glGetString(GL_EXTENSIONS);
  if (!extensionsData) {
    cachedSupport = 0;
    return false;
  }

  const std::string extensions(
      reinterpret_cast<const char *>(extensionsData));
  const bool hasPboExtension =
      extensions.find("GL_ARB_pixel_buffer_object") != std::string::npos;
  cachedSupport = hasPboExtension ? 1 : 0;
  return hasPboExtension;
}


// Represents an immutable CPU-produced RGBA payload ready for GPU upload.
struct PreparedRgbaPayload {
  bool valid = false;
  int width = 0;
  int height = 0;
  std::vector<unsigned char> pixels;
};

// Converts a wxImage into RGBA bytes that can be uploaded on the render thread.
PreparedRgbaPayload PrepareRgbaPayloadFromImage(wxImage image, const char *context) {
  PreparedRgbaPayload payload;
  if (!image.IsOk())
    return payload;
  image = image.Mirror(false);
  if (!image.HasAlpha())
    image.InitAlpha();
  const int width = image.GetWidth();
  const int height = image.GetHeight();
  const unsigned char *rgb = image.GetData();
  const unsigned char *alpha = image.GetAlpha();
  if (!rgb || width <= 0 || height <= 0 || !TryAllocatePixelBuffer(payload.pixels, width, height, context))
    return payload;
  for (int i = 0; i < width * height; ++i) {
    payload.pixels[static_cast<size_t>(i) * 4] = rgb[i * 3];
    payload.pixels[static_cast<size_t>(i) * 4 + 1] = rgb[i * 3 + 1];
    payload.pixels[static_cast<size_t>(i) * 4 + 2] = rgb[i * 3 + 2];
    payload.pixels[static_cast<size_t>(i) * 4 + 3] = alpha ? alpha[i] : 255;
  }
  payload.width = width;
  payload.height = height;
  payload.valid = true;
  return payload;
}

// Ensures the pixel-unpack PBO exists and has enough storage for the upload.
bool EnsurePboCapacity(unsigned int &pbo, size_t &capacity, size_t bytesNeeded) {
  if (bytesNeeded == 0)
    return false;
  if (pbo == 0)
    glGenBuffers(1, &pbo);
  if (pbo == 0)
    return false;

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
  if (capacity < bytesNeeded) {
    glBufferData(GL_PIXEL_UNPACK_BUFFER, static_cast<GLsizeiptr>(bytesNeeded),
                 nullptr, GL_STREAM_DRAW);
    capacity = bytesNeeded;
  }
  return true;
}

// Returns whether this runtime should use PBO-based texture uploads.
bool ShouldUsePboTextureUpload() {
#if defined(__linux__)
  // Linux AppImage GPU/driver combinations have shown intermittent blank
  // layout view textures when PBO uploads are enabled, so prefer direct uploads.
  return false;
#else
  return true;
#endif
}

// Uploads RGBA pixels into the currently bound texture and reallocates if needed.
bool UploadRgbaToTexture(unsigned int texture, int width, int height,
                         const unsigned char *data,
                         const wxSize &currentTextureSize, bool allowPbo) {
  if (texture == 0 || width <= 0 || height <= 0 || data == nullptr)
    return false;

  const bool needsAllocation =
      currentTextureSize.GetWidth() != width ||
      currentTextureSize.GetHeight() != height;

  if (needsAllocation) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
  }

  const size_t bytesNeeded =
      static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
  bool uploaded = false;

  if (allowPbo && ShouldUsePboTextureUpload() && IsPixelUnpackPboSupported() &&
      gActivePixelUnpackPbo &&
      gActivePixelUnpackPboBytes &&
      EnsurePboCapacity(*gActivePixelUnpackPbo, *gActivePixelUnpackPboBytes,
                        bytesNeeded)) {
    void *mappedBuffer = nullptr;
#if defined(GL_MAP_INVALIDATE_BUFFER_BIT) && defined(GL_MAP_WRITE_BIT)
    mappedBuffer =
        glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0,
                         static_cast<GLsizeiptr>(bytesNeeded),
                         GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
#endif
    if (!mappedBuffer) {
      mappedBuffer = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    }

    if (mappedBuffer) {
      std::memcpy(mappedBuffer, data, bytesNeeded);
      if (glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER) == GL_TRUE) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA,
                        GL_UNSIGNED_BYTE, nullptr);
        uploaded = true;
      }
    }
  }

  if (!uploaded) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA,
                    GL_UNSIGNED_BYTE, data);
    uploaded = true;
  }

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  return uploaded;
}

// Snaps an integer coordinate to the configured layout grid step.
int SnapToGrid(int value) {
  if (kLayoutGridStep <= 1)
    return value;
  return static_cast<int>(
      std::lround(static_cast<double>(value) / kLayoutGridStep) *
      kLayoutGridStep);
}
} // namespace

// Queues a textured quad for deferred VBO/VAO submission while preserving draw order by texture runs.
void LayoutViewerPanel::QueueTexturedQuad(GLuint texture,
                                          const wxRect &frameRect) {
  if (texture == 0)
    return;
  const int frameRight = frameRect.GetLeft() + frameRect.GetWidth();
  const int frameBottom = frameRect.GetTop() + frameRect.GetHeight();
  TexturedQuadBatch *batch = nullptr;
  if (!texturedQuadBatches_.empty() &&
      texturedQuadBatches_.back().texture == texture) {
    batch = &texturedQuadBatches_.back();
  } else {
    texturedQuadBatches_.push_back(TexturedQuadBatch{});
    texturedQuadBatches_.back().texture = texture;
    batch = &texturedQuadBatches_.back();
  }
  batch->vertices.push_back({static_cast<float>(frameRect.GetLeft()),
                             static_cast<float>(frameRect.GetTop()), 0.0f,
                             1.0f});
  batch->vertices.push_back({static_cast<float>(frameRight),
                             static_cast<float>(frameRect.GetTop()), 1.0f,
                             1.0f});
  batch->vertices.push_back({static_cast<float>(frameRight),
                             static_cast<float>(frameBottom), 1.0f, 0.0f});
  batch->vertices.push_back({static_cast<float>(frameRect.GetLeft()),
                             static_cast<float>(frameRect.GetBottom()), 0.0f,
                             0.0f});
}

// Draws one textured quad using legacy immediate mode as the compatibility fallback path.
void LayoutViewerPanel::DrawTexturedQuadImmediate(GLuint texture,
                                                  const wxRect &frameRect) const {
  const int frameRight = frameRect.GetLeft() + frameRect.GetWidth();
  const int frameBottom = frameRect.GetTop() + frameRect.GetHeight();
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, texture);
  glColor4ub(255, 255, 255, 255);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetTop()));
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(static_cast<float>(frameRight),
             static_cast<float>(frameRect.GetTop()));
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(static_cast<float>(frameRight), static_cast<float>(frameBottom));
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetBottom()));
  glEnd();
  glDisable(GL_TEXTURE_2D);
}

// Flushes queued textured quads using a VBO/VAO path and falls back to immediate mode when unavailable.
void LayoutViewerPanel::FlushQueuedTexturedQuads() {
  if (texturedQuadBatches_.empty())
    return;
  if (texturedQuadVao_ == 0)
    glGenVertexArrays(1, &texturedQuadVao_);
  if (texturedQuadVbo_ == 0)
    glGenBuffers(1, &texturedQuadVbo_);
  const bool canUseVboVao = texturedQuadVao_ != 0 && texturedQuadVbo_ != 0;
  if (!canUseVboVao) {
    for (const auto &batch : texturedQuadBatches_) {
      for (size_t i = 0; i + 3 < batch.vertices.size(); i += 4) {
        wxRect r(static_cast<int>(batch.vertices[i].x),
                 static_cast<int>(batch.vertices[i].y),
                 static_cast<int>(batch.vertices[i + 1].x -
                                  batch.vertices[i].x),
                 static_cast<int>(batch.vertices[i + 2].y -
                                  batch.vertices[i].y));
        DrawTexturedQuadImmediate(batch.texture, r);
      }
    }
    texturedQuadBatches_.clear();
    return;
  }
  glEnable(GL_TEXTURE_2D);
  glColor4ub(255, 255, 255, 255);
  glBindVertexArray(texturedQuadVao_);
  glBindBuffer(GL_ARRAY_BUFFER, texturedQuadVbo_);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  for (const auto &batch : texturedQuadBatches_) {
    glBindTexture(GL_TEXTURE_2D, batch.texture);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(batch.vertices.size() *
                                         sizeof(TexturedQuadVertex)),
                 batch.vertices.data(), GL_STREAM_DRAW);
    glVertexPointer(2, GL_FLOAT, sizeof(TexturedQuadVertex),
                    reinterpret_cast<const void *>(
                        offsetof(TexturedQuadVertex, x)));
    glTexCoordPointer(2, GL_FLOAT, sizeof(TexturedQuadVertex),
                      reinterpret_cast<const void *>(
                          offsetof(TexturedQuadVertex, u)));
    glDrawArrays(GL_QUADS, 0, static_cast<GLsizei>(batch.vertices.size()));
  }
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  glDisable(GL_TEXTURE_2D);
  texturedQuadBatches_.clear();
}

wxDEFINE_EVENT(EVT_LAYOUT_VIEW_EDIT, wxCommandEvent);

wxBEGIN_EVENT_TABLE(LayoutViewerPanel, wxGLCanvas)
    EVT_PAINT(LayoutViewerPanel::OnPaint)
    EVT_SIZE(LayoutViewerPanel::OnSize)
    EVT_LEFT_DOWN(LayoutViewerPanel::OnLeftDown)
    EVT_LEFT_UP(LayoutViewerPanel::OnLeftUp)
    EVT_LEFT_DCLICK(LayoutViewerPanel::OnLeftDClick)
    EVT_MOTION(LayoutViewerPanel::OnMouseMove)
    EVT_MOUSEWHEEL(LayoutViewerPanel::OnMouseWheel)
    EVT_MOUSE_CAPTURE_LOST(LayoutViewerPanel::OnCaptureLost)
    EVT_RIGHT_UP(LayoutViewerPanel::OnRightUp)
    EVT_KEY_DOWN(LayoutViewerPanel::OnKeyDown)
    EVT_SHOW(LayoutViewerPanel::OnShow)
    EVT_MENU(kEditMenuId, LayoutViewerPanel::OnEditView)
    EVT_MENU(kDeleteMenuId, LayoutViewerPanel::OnDeleteView)
    EVT_MENU(kToggleViewFrameMenuId, LayoutViewerPanel::OnToggleViewFrame)
    EVT_MENU(kEditLegendMenuId, LayoutViewerPanel::OnEditLegend)
    EVT_MENU(kDeleteLegendMenuId, LayoutViewerPanel::OnDeleteLegend)
    EVT_MENU(kEditEventTableMenuId, LayoutViewerPanel::OnEditEventTable)
    EVT_MENU(kDeleteEventTableMenuId, LayoutViewerPanel::OnDeleteEventTable)
    EVT_MENU(kEditTextMenuId, LayoutViewerPanel::OnEditText)
    EVT_MENU(kDeleteTextMenuId, LayoutViewerPanel::OnDeleteText)
    EVT_MENU(kToggleTextFrameMenuId, LayoutViewerPanel::OnToggleTextFrame)
    EVT_MENU(kToggleTextTransparentBackgroundMenuId,
             LayoutViewerPanel::OnToggleTextTransparentBackground)
    EVT_MENU(kEditImageMenuId, LayoutViewerPanel::OnEditImage)
    EVT_MENU(kDeleteImageMenuId, LayoutViewerPanel::OnDeleteImage)
    EVT_MENU(kBringToFrontMenuId, LayoutViewerPanel::OnBringToFront)
    EVT_MENU(kSendToBackMenuId, LayoutViewerPanel::OnSendToBack)
wxEND_EVENT_TABLE()

wxDEFINE_EVENT(EVT_LAYOUT_RENDER_READY, wxCommandEvent);
wxDEFINE_EVENT(EVT_LAYOUT_VIEW_SELECTED, wxCommandEvent);

LayoutViewerPanel::LayoutViewerPanel(wxWindow *parent)
    : wxGLCanvas(parent, wxID_ANY, nullptr, wxDefaultPosition,
                 wxDefaultSize,
                 wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS) {
  SetBackgroundStyle(wxBG_STYLE_CUSTOM);
  glContext_ = new wxGLContext(this);
  currentLayout.pageSetup.pageSize = print::PageSize::A4;
  currentLayout.pageSetup.landscape = true;
  pendingFitOnResize = true;
  loadingTimer_.SetOwner(this, kLoadingTimerId);
  renderDelayTimer_.SetOwner(this, kRenderDelayTimerId);
  Bind(wxEVT_TIMER, &LayoutViewerPanel::OnLoadingTimer, this, kLoadingTimerId);
  Bind(wxEVT_TIMER, &LayoutViewerPanel::OnRenderDelayTimer, this,
       kRenderDelayTimerId);
  ResetViewToFit();
}

LayoutViewerPanel::~LayoutViewerPanel() {
  if (HasCapture()) {
    ReleaseMouse();
  }
  Unbind(wxEVT_TIMER, &LayoutViewerPanel::OnLoadingTimer, this,
         kLoadingTimerId);
  Unbind(wxEVT_TIMER, &LayoutViewerPanel::OnRenderDelayTimer, this,
         kRenderDelayTimerId);
  ClearCachedTexture();
  ClearLoadingTextTexture();
  loadingTimer_.Stop();
  renderDelayTimer_.Stop();
  delete glContext_;
}

void LayoutViewerPanel::SetLayoutDefinition(
    const layouts::LayoutDefinition &layout) {
  // Advances the render epoch so pending worker payloads from older layouts are discarded.
  NextRenderEpoch();
  if (IsSameRenderableLayout(currentLayout, layout)) {
    currentLayout.name = layout.name;
    legendDataDirty_ = true;
    RefreshLegendData();
    InvalidateRenderIfFrameChanged(true);
    if (NeedsRenderRebuild())
      RequestRenderRebuild();
    Refresh();
    NotifyRenderReady();
    return;
  }

  const layouts::LayoutDefinition previousLayout = currentLayout;
  const bool sameLayoutName =
      !previousLayout.name.empty() && previousLayout.name == layout.name;
  currentLayout = layout;
  InvalidateSelectionIndexCache();
  auto selectDefaultElement = [this]() {
    if (!currentLayout.view2dViews.empty()) {
      selectedElementType = SelectedElementType::View2D;
      selectedElementId = currentLayout.view2dViews.front().id;
      EmitViewSelectionChanged(selectedElementId);
    } else if (!currentLayout.legendViews.empty()) {
      selectedElementType = SelectedElementType::Legend;
      selectedElementId = currentLayout.legendViews.front().id;
    } else if (!currentLayout.eventTables.empty()) {
      selectedElementType = SelectedElementType::EventTable;
      selectedElementId = currentLayout.eventTables.front().id;
    } else if (!currentLayout.textViews.empty()) {
      selectedElementType = SelectedElementType::Text;
      selectedElementId = currentLayout.textViews.front().id;
    } else if (!currentLayout.imageViews.empty()) {
      selectedElementType = SelectedElementType::Image;
      selectedElementId = currentLayout.imageViews.front().id;
    } else {
      selectedElementType = SelectedElementType::None;
      selectedElementId = -1;
    }
  };

  auto hasSelectedElement = [this]() {
    if (selectedElementId < 0)
      return false;
    if (selectedElementType == SelectedElementType::View2D) {
      return std::any_of(currentLayout.view2dViews.begin(),
                         currentLayout.view2dViews.end(),
                         [this](const auto &entry) {
                           return entry.id == selectedElementId;
                         });
    }
    if (selectedElementType == SelectedElementType::Legend) {
      return std::any_of(currentLayout.legendViews.begin(),
                         currentLayout.legendViews.end(),
                         [this](const auto &entry) {
                           return entry.id == selectedElementId;
                         });
    }
    if (selectedElementType == SelectedElementType::EventTable) {
      return std::any_of(currentLayout.eventTables.begin(),
                         currentLayout.eventTables.end(),
                         [this](const auto &entry) {
                           return entry.id == selectedElementId;
                         });
    }
    if (selectedElementType == SelectedElementType::Text) {
      return std::any_of(currentLayout.textViews.begin(),
                         currentLayout.textViews.end(),
                         [this](const auto &entry) {
                           return entry.id == selectedElementId;
                         });
    }
    if (selectedElementType == SelectedElementType::Image) {
      return std::any_of(currentLayout.imageViews.begin(),
                         currentLayout.imageViews.end(),
                         [this](const auto &entry) {
                           return entry.id == selectedElementId;
                         });
    }
    return false;
  };

  if (!hasSelectedElement()) {
    selectDefaultElement();
  }
  layoutVersion++;
  if (!AreEqual(previousLayout.view2dViews, currentLayout.view2dViews)) {
    captureInProgress = false;
  }
  pendingFrameCommit_ = false;

  if (sameLayoutName) {
    captureInProgress = false;
    contentDirty = true;
    loadingRequested = true;
    legendDataDirty_ = true;
    RefreshLegendData();
    InvalidateRenderIfFrameChanged(false);
    if (NeedsRenderRebuild())
      RequestRenderRebuild();
    Refresh();
    return;
  }

  MaybePrewarmOnLayoutModeEntry(previousLayout);
  captureInProgress = false;
  ClearCachedTexture();
  const bool emptyLayout = IsLayoutEmpty();
  if (emptyLayout) {
    selectedElementType = SelectedElementType::None;
    selectedElementId = -1;
    renderDirty = false;
    contentDirty = false;
    presentationDirty = false;
    loadingRequested = false;
    isLoading = false;
    legendItems_.clear();
    legendDataHash = 0;
    legendDataDirty_ = false;
    pendingFitOnResize = true;
    ResetViewToFit();
    Refresh();
    NotifyRenderReady();
    return;
  }
  contentDirty = true;
  loadingRequested = true;
  legendDataDirty_ = true;
  RefreshLegendData();
  pendingFitOnResize = true;
  ResetViewToFit();
  RequestRenderRebuild();
  Refresh();
}

// Prewarms reusable legend symbols and view render states when switching into a new layout context.
void LayoutViewerPanel::MaybePrewarmOnLayoutModeEntry(const layouts::LayoutDefinition &previousLayout) {
  if (currentLayout.name.empty())
    return;
  const bool likelyModeEntryFrom2Dor3D =
      previousLayout.name.empty() || previousLayout.name != currentLayout.name;
  if (!likelyModeEntryFrom2Dor3D)
    return;

  const std::string prewarmKey =
      currentLayout.name + "#" + std::to_string(layoutVersion);
  PrewarmedLayoutArtifacts artifacts;
  artifacts.layoutVersion = layoutVersion;
  artifacts.contentHash = BuildLayoutPrewarmContentHash();
  for (const auto &view : currentLayout.view2dViews) {
    viewer2d::Viewer2DState state = viewer2d::FromLayoutDefinition(view);
    state.renderOptions.darkMode = false;
    artifacts.viewRenderStates.emplace(view.id, std::move(state));
  }

  Viewer2DPanel *capturePanel = nullptr;
  if (auto *mw = MainWindow::Instance()) {
    if (auto *offscreenRenderer = mw->GetOffscreenRenderer())
      capturePanel = offscreenRenderer->GetPanel();
  }
  if (!capturePanel)
    capturePanel = Viewer2DPanel::Instance();
  if (capturePanel) {
    ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
    artifacts.legendSymbols = CaptureLegendSymbolSnapshot(capturePanel, cfg, true);
  }

  prewarmedArtifactsByLayout_[prewarmKey] = std::move(artifacts);
  ApplyPrewarmedArtifactsIfAvailable();
}

// Applies prewarmed artifacts to current caches when layout identity and content hash still match.
void LayoutViewerPanel::ApplyPrewarmedArtifactsIfAvailable() {
  const std::string prewarmKey =
      currentLayout.name + "#" + std::to_string(layoutVersion);
  auto it = prewarmedArtifactsByLayout_.find(prewarmKey);
  if (it == prewarmedArtifactsByLayout_.end())
    return;
  const PrewarmedLayoutArtifacts &artifacts = it->second;
  if (artifacts.layoutVersion != layoutVersion ||
      artifacts.contentHash != BuildLayoutPrewarmContentHash()) {
    prewarmedArtifactsByLayout_.erase(it);
    return;
  }

  for (const auto &view : currentLayout.view2dViews) {
    auto stateIt = artifacts.viewRenderStates.find(view.id);
    if (stateIt == artifacts.viewRenderStates.end())
      continue;
    ViewCache &cache = GetViewCache(view.id);
    cache.renderState = stateIt->second;
    cache.hasRenderState = true;
  }
  if (artifacts.legendSymbols) {
    for (const auto &legend : currentLayout.legendViews) {
      LegendCache &cache = GetLegendCache(legend.id);
      cache.symbols = artifacts.legendSymbols;
      cache.renderDirty = true;
    }
  }
}

// Builds a stable invalidation hash used to discard prewarm entries after layout name/content/version changes.
size_t LayoutViewerPanel::BuildLayoutPrewarmContentHash() const {
  size_t seed = std::hash<std::string>{}(currentLayout.name);
  const size_t sceneHash = ComputeSceneContentHash();
  seed ^= sceneHash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  seed ^= std::hash<int>{}(layoutVersion) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  return seed;
}

void LayoutViewerPanel::NotifyRenderReady() {
  wxWeakRef<LayoutViewerPanel> weakThis(this);
  CallAfter([weakThis]() {
    if (!weakThis)
      return;
    LayoutViewerPanel *panel = weakThis.get();
    if (!panel)
      return;
    wxCommandEvent event(EVT_LAYOUT_RENDER_READY);
    event.SetEventObject(panel);
    wxPostEvent(panel, event);
  });
}

void LayoutViewerPanel::InvalidateSelectionIndexCache() {
  selectionIndexCache_.dirty = true;
}

void LayoutViewerPanel::EnsureSelectionIndexCache() {
  if (!selectionIndexCache_.dirty) {
    return;
  }

  selectionIndexCache_.zOrderedElements = BuildZOrderedElements();

  auto &viewById = selectionIndexCache_.viewById;
  auto &legendById = selectionIndexCache_.legendById;
  auto &eventTableById = selectionIndexCache_.eventTableById;
  auto &textById = selectionIndexCache_.textById;
  auto &imageById = selectionIndexCache_.imageById;

  viewById.clear();
  legendById.clear();
  eventTableById.clear();
  textById.clear();
  imageById.clear();

  viewById.reserve(currentLayout.view2dViews.size());
  legendById.reserve(currentLayout.legendViews.size());
  eventTableById.reserve(currentLayout.eventTables.size());
  textById.reserve(currentLayout.textViews.size());
  imageById.reserve(currentLayout.imageViews.size());

  for (const auto &view : currentLayout.view2dViews)
    viewById.emplace(view.id, &view);
  for (const auto &legend : currentLayout.legendViews)
    legendById.emplace(legend.id, &legend);
  for (const auto &table : currentLayout.eventTables)
    eventTableById.emplace(table.id, &table);
  for (const auto &text : currentLayout.textViews)
    textById.emplace(text.id, &text);
  for (const auto &image : currentLayout.imageViews)
    imageById.emplace(image.id, &image);

  selectionIndexCache_.dirty = false;
}

std::vector<LayoutViewerPanel::ZOrderedElement>
LayoutViewerPanel::BuildZOrderedElements() const {
  std::vector<ZOrderedElement> elements;
  elements.reserve(currentLayout.view2dViews.size() +
                   currentLayout.legendViews.size() +
                   currentLayout.eventTables.size() +
                   currentLayout.textViews.size() +
                   currentLayout.imageViews.size());
  size_t order = 0;
  for (const auto &view : currentLayout.view2dViews) {
    elements.push_back(
        {SelectedElementType::View2D, view.id, view.zIndex, order++});
  }
  for (const auto &legend : currentLayout.legendViews) {
    elements.push_back(
        {SelectedElementType::Legend, legend.id, legend.zIndex, order++});
  }
  for (const auto &table : currentLayout.eventTables) {
    elements.push_back(
        {SelectedElementType::EventTable, table.id, table.zIndex, order++});
  }
  for (const auto &text : currentLayout.textViews) {
    elements.push_back(
        {SelectedElementType::Text, text.id, text.zIndex, order++});
  }
  for (const auto &image : currentLayout.imageViews) {
    elements.push_back(
        {SelectedElementType::Image, image.id, image.zIndex, order++});
  }
  std::stable_sort(elements.begin(), elements.end(),
                   [](const auto &lhs, const auto &rhs) {
                     if (lhs.zIndex != rhs.zIndex)
                       return lhs.zIndex < rhs.zIndex;
                     return lhs.order < rhs.order;
                   });
  return elements;
}

std::pair<int, int> LayoutViewerPanel::GetZIndexRange() const {
  bool hasValue = false;
  int minZ = 0;
  int maxZ = 0;
  for (const auto &view : currentLayout.view2dViews) {
    if (!hasValue) {
      minZ = view.zIndex;
      maxZ = view.zIndex;
      hasValue = true;
    } else {
      minZ = std::min(minZ, view.zIndex);
      maxZ = std::max(maxZ, view.zIndex);
    }
  }
  for (const auto &legend : currentLayout.legendViews) {
    if (!hasValue) {
      minZ = legend.zIndex;
      maxZ = legend.zIndex;
      hasValue = true;
    } else {
      minZ = std::min(minZ, legend.zIndex);
      maxZ = std::max(maxZ, legend.zIndex);
    }
  }
  for (const auto &table : currentLayout.eventTables) {
    if (!hasValue) {
      minZ = table.zIndex;
      maxZ = table.zIndex;
      hasValue = true;
    } else {
      minZ = std::min(minZ, table.zIndex);
      maxZ = std::max(maxZ, table.zIndex);
    }
  }
  for (const auto &text : currentLayout.textViews) {
    if (!hasValue) {
      minZ = text.zIndex;
      maxZ = text.zIndex;
      hasValue = true;
    } else {
      minZ = std::min(minZ, text.zIndex);
      maxZ = std::max(maxZ, text.zIndex);
    }
  }
  for (const auto &image : currentLayout.imageViews) {
    if (!hasValue) {
      minZ = image.zIndex;
      maxZ = image.zIndex;
      hasValue = true;
    } else {
      minZ = std::min(minZ, image.zIndex);
      maxZ = std::max(maxZ, image.zIndex);
    }
  }
  return {minZ, maxZ};
}

bool LayoutViewerPanel::IsLayoutEmpty() const {
  return currentLayout.view2dViews.empty() &&
         currentLayout.legendViews.empty() &&
         currentLayout.eventTables.empty() && currentLayout.textViews.empty() &&
         currentLayout.imageViews.empty();
}

void LayoutViewerPanel::OnPaint(wxPaintEvent &) {
  static unsigned long long s_renderFrameId = 0;
  wxPaintDC dc(this);
  try {
    if (!IsShownOnScreen()) {
      return;
    }
    if (!InitGL()) {
      return;
    }
    InitGL();
    if (!isReadyToRender_) {
      return;
    }
    SetCurrent(*glContext_);
    if (legendDataDirty_)
      RefreshLegendData();
    if (!renderPending && NeedsRenderRebuild()) {
      RequestRenderRebuild();
    }

    const wxSize logicalSize = GetLogicalClientSize(this);
    if (logicalSize.GetWidth() <= 0 || logicalSize.GetHeight() <= 0) {
      return;
    }
    const RenderSize resolvedSize = ResolveRenderSize(this);
    if (!resolvedSize.IsValid()) {
      return;
    }
    const wxSize framebufferSize(resolvedSize.width, resolvedSize.height);
    glstate::ApplyKnownBaseOnscreenState(framebufferSize.GetWidth(),
                                         framebufferSize.GetHeight());
    const RenderSize viewportSize{framebufferSize.GetWidth(),
                                  framebufferSize.GetHeight(),
                                  "glstate::ApplyKnownBaseOnscreenState(framebuffer-px)"};
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, logicalSize.GetWidth(), logicalSize.GetHeight(), 0.0, -1.0,
            1.0);
    const wxPoint projectionFramebufferPoint =
        ToFramebufferPoint(this,
                           wxPoint(logicalSize.GetWidth(), logicalSize.GetHeight()));
    const RenderSize projectionSize{
        projectionFramebufferPoint.x, projectionFramebufferPoint.y,
        "LayoutViewerPanel::OnPaint::ortho(logical-dip mapped to framebuffer-px)"};
    ++s_renderFrameId;
    ValidateRenderSizeContract("LayoutViewerPanel", s_renderFrameId,
                               resolvedSize, viewportSize, projectionSize);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.35f, 0.35f, 0.35f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const double pageWidth = currentLayout.pageSetup.PageWidthPt();
    const double pageHeight = currentLayout.pageSetup.PageHeightPt();

    const double scaledWidth = pageWidth * zoom;
    const double scaledHeight = pageHeight * zoom;

    const wxPoint center(logicalSize.GetWidth() / 2, logicalSize.GetHeight() / 2);
    const wxPoint topLeft(center.x - static_cast<int>(scaledWidth / 2.0) +
                              panOffset.x,
                          center.y - static_cast<int>(scaledHeight / 2.0) +
                              panOffset.y);

    glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
    glVertex2f(static_cast<float>(topLeft.x), static_cast<float>(topLeft.y));
    glVertex2f(static_cast<float>(topLeft.x + scaledWidth),
               static_cast<float>(topLeft.y));
    glVertex2f(static_cast<float>(topLeft.x + scaledWidth),
               static_cast<float>(topLeft.y + scaledHeight));
    glVertex2f(static_cast<float>(topLeft.x),
               static_cast<float>(topLeft.y + scaledHeight));
    glEnd();

    glColor4ub(200, 200, 200, 255);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(static_cast<float>(topLeft.x), static_cast<float>(topLeft.y));
    glVertex2f(static_cast<float>(topLeft.x + scaledWidth),
               static_cast<float>(topLeft.y));
    glVertex2f(static_cast<float>(topLeft.x + scaledWidth),
               static_cast<float>(topLeft.y + scaledHeight));
    glVertex2f(static_cast<float>(topLeft.x),
               static_cast<float>(topLeft.y + scaledHeight));
    glEnd();

    const layouts::Layout2DViewDefinition *activeView =
        static_cast<const LayoutViewerPanel *>(this)->GetEditableView();
    const bool showDeferredResizeOverlay =
        deferredResizeFrame_.has_value() && dragMode != FrameDragMode::None &&
        dragMode != FrameDragMode::Move;
    const int selectedViewId =
        selectedElementType == SelectedElementType::View2D && activeView
            ? activeView->id
            : -1;
    const int selectedLegendId =
        selectedElementType == SelectedElementType::Legend ? selectedElementId
                                                           : -1;
    const int selectedEventTableId =
        selectedElementType == SelectedElementType::EventTable
            ? selectedElementId
            : -1;
    const int selectedTextId =
        selectedElementType == SelectedElementType::Text ? selectedElementId
                                                         : -1;
    const int selectedImageId =
        selectedElementType == SelectedElementType::Image ? selectedElementId
                                                          : -1;

    const int activeViewId = showDeferredResizeOverlay ? -1 : selectedViewId;
    const int activeLegendId =
        showDeferredResizeOverlay ? -1 : selectedLegendId;
    const int activeEventTableId =
        showDeferredResizeOverlay ? -1 : selectedEventTableId;
    const int activeTextId = showDeferredResizeOverlay ? -1 : selectedTextId;
    const int activeImageId =
        showDeferredResizeOverlay ? -1 : selectedImageId;

    Viewer2DPanel *capturePanel = nullptr;
    Viewer2DOffscreenRenderer *offscreenRenderer = nullptr;
    if (auto *mw = MainWindow::Instance()) {
      offscreenRenderer = mw->GetOffscreenRenderer();
      capturePanel =
          offscreenRenderer ? offscreenRenderer->GetPanel() : nullptr;
    } else {
      capturePanel = Viewer2DPanel::Instance();
    }

    std::unordered_map<int, const layouts::Layout2DViewDefinition *> viewById;
    std::unordered_map<int, const layouts::LayoutLegendDefinition *>
        legendById;
    std::unordered_map<int, const layouts::LayoutEventTableDefinition *>
        eventTableById;
    std::unordered_map<int, const layouts::LayoutTextDefinition *> textById;
    std::unordered_map<int, const layouts::LayoutImageDefinition *> imageById;
    viewById.reserve(currentLayout.view2dViews.size());
    legendById.reserve(currentLayout.legendViews.size());
    eventTableById.reserve(currentLayout.eventTables.size());
    textById.reserve(currentLayout.textViews.size());
    imageById.reserve(currentLayout.imageViews.size());
    for (const auto &view : currentLayout.view2dViews)
      viewById.emplace(view.id, &view);
    for (const auto &legend : currentLayout.legendViews)
      legendById.emplace(legend.id, &legend);
    for (const auto &table : currentLayout.eventTables)
      eventTableById.emplace(table.id, &table);
    for (const auto &text : currentLayout.textViews)
      textById.emplace(text.id, &text);
    for (const auto &image : currentLayout.imageViews)
      imageById.emplace(image.id, &image);

    const auto elements = BuildZOrderedElements();
    for (const auto &element : elements) {
      if (element.type == SelectedElementType::View2D) {
        auto it = viewById.find(element.id);
        if (it != viewById.end())
          DrawViewElement(*it->second, capturePanel, offscreenRenderer,
                          activeViewId);
      } else if (element.type == SelectedElementType::Legend) {
        auto it = legendById.find(element.id);
        if (it != legendById.end())
          DrawLegendElement(*it->second, activeLegendId);
      } else if (element.type == SelectedElementType::EventTable) {
        auto it = eventTableById.find(element.id);
        if (it != eventTableById.end())
          DrawEventTableElement(*it->second);
      } else if (element.type == SelectedElementType::Text) {
        auto it = textById.find(element.id);
        if (it != textById.end())
          DrawTextElement(*it->second, activeTextId);
      } else if (element.type == SelectedElementType::Image) {
        auto it = imageById.find(element.id);
        if (it != imageById.end())
          DrawImageElement(*it->second, activeImageId);
      }
    }

    if (showDeferredResizeOverlay)
      DrawDeferredResizeOverlay();

    const bool texturesReady = AreTexturesReady();
    auto hasTexture = [](const auto &cacheMap, int id) {
      auto it = cacheMap.find(id);
      return it != cacheMap.end() && it->second.texture != 0;
    };
    bool activeElementHasTexture = false;
    if (selectedElementType == SelectedElementType::View2D) {
      if (!activeView) {
        activeElementHasTexture = false;
      } else {
        activeElementHasTexture = hasTexture(viewCaches_, selectedViewId);
      }
    } else if (selectedElementType == SelectedElementType::Legend) {
      auto it = legendById.find(selectedLegendId);
      if (it == legendById.end()) {
        activeElementHasTexture = false;
      } else {
        activeElementHasTexture = hasTexture(legendCaches_, selectedLegendId);
      }
    } else if (selectedElementType == SelectedElementType::EventTable) {
      auto it = eventTableById.find(selectedEventTableId);
      if (it == eventTableById.end()) {
        activeElementHasTexture = false;
      } else {
        activeElementHasTexture =
            hasTexture(eventTableCaches_, selectedEventTableId);
      }
    } else if (selectedElementType == SelectedElementType::Text) {
      auto it = textById.find(selectedTextId);
      if (it == textById.end()) {
        activeElementHasTexture = false;
      } else {
        activeElementHasTexture = hasTexture(textCaches_, selectedTextId);
      }
    } else if (selectedElementType == SelectedElementType::Image) {
      auto it = imageById.find(selectedImageId);
      if (it == imageById.end()) {
        activeElementHasTexture = false;
      } else {
        activeElementHasTexture = hasTexture(imageCaches_, selectedImageId);
      }
    }
    const auto hasAnyTexture = [this]() {
      auto hasAny = [](const auto &map) {
        for (const auto &entry : map) {
          if (entry.second.texture != 0)
            return true;
        }
        return false;
      };
      return hasAny(viewCaches_) || hasAny(legendCaches_) ||
             hasAny(eventTableCaches_) || hasAny(textCaches_) ||
             hasAny(imageCaches_);
    };
    const bool showLoadingOverlay =
        !IsLayoutEmpty() && isLoading && !hasAnyTexture() &&
        (!texturesReady || !activeElementHasTexture);
    if (showLoadingOverlay && isReadyToRender_) {
      DrawLoadingOverlay(logicalSize);
    }

    glFlush();
    ValidateGlStateAfterRender("LayoutViewerPanel::OnPaint",
                               framebufferSize.GetWidth(),
                               framebufferSize.GetHeight());
    SwapBuffers();
  } catch (const std::exception &ex) {
    Logger::Instance().Log(
        std::string("LayoutViewerPanel::OnPaint exception: ") + ex.what());
  } catch (...) {
    Logger::Instance().Log("LayoutViewerPanel::OnPaint unknown exception.");
  }
}

void LayoutViewerPanel::DrawLoadingOverlay(const wxSize &size) {
  if (!glContext_ || !isReadyToRender_)
    return;
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColor4ub(0, 0, 0, 150);
  glBegin(GL_QUADS);
  glVertex2f(0.0f, 0.0f);
  glVertex2f(static_cast<float>(size.GetWidth()), 0.0f);
  glVertex2f(static_cast<float>(size.GetWidth()),
             static_cast<float>(size.GetHeight()));
  glVertex2f(0.0f, static_cast<float>(size.GetHeight()));
  glEnd();

  EnsureLoadingTextTexture();
  if (loadingTextTexture_ == 0)
    return;

  const int textWidth = loadingTextTextureSize_.GetWidth();
  const int textHeight = loadingTextTextureSize_.GetHeight();
  if (textWidth <= 0 || textHeight <= 0)
    return;

  const float x = (size.GetWidth() - textWidth) * 0.5f;
  const float y = (size.GetHeight() - textHeight) * 0.5f;

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, loadingTextTexture_);
  glColor4ub(255, 255, 255, 255);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(x, y);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(x + textWidth, y);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(x + textWidth, y + textHeight);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(x, y + textHeight);
  glEnd();
  glDisable(GL_TEXTURE_2D);
}

void LayoutViewerPanel::DrawDeferredResizeOverlay() {
  if (!deferredResizeFrame_.has_value())
    return;
  if (dragMode == FrameDragMode::None || dragMode == FrameDragMode::Move)
    return;

  wxRect frameRect;
  if (!GetFrameRect(*deferredResizeFrame_, frameRect))
    return;

  glColor4ub(160, 160, 160, 110);
  glBegin(GL_QUADS);
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetTop()));
  glVertex2f(static_cast<float>(frameRect.GetRight()),
             static_cast<float>(frameRect.GetTop()));
  glVertex2f(static_cast<float>(frameRect.GetRight()),
             static_cast<float>(frameRect.GetBottom()));
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetBottom()));
  glEnd();

  glColor4ub(0, 128, 255, 255);
  glLineWidth(2.0f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetTop()));
  glVertex2f(static_cast<float>(frameRect.GetRight()),
             static_cast<float>(frameRect.GetTop()));
  glVertex2f(static_cast<float>(frameRect.GetRight()),
             static_cast<float>(frameRect.GetBottom()));
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetBottom()));
  glEnd();

  DrawSelectionHandles(frameRect);
}

void LayoutViewerPanel::EnsureLoadingTextTexture() {
  if (loadingTextTexture_ != 0) {
    if (!InitGL())
      return;
    if (!glIsTexture(loadingTextTexture_)) {
      loadingTextTexture_ = 0;
      loadingTextTextureSize_ = wxSize(0, 0);
    } else {
      return;
    }
  }

  const wxString label = wxString::FromUTF8("Loading layout...");
  wxFont font = wxFontInfo(14).Bold();
  int textWidth = 0;
  int textHeight = 0;
  {
    wxMemoryDC measureDc;
    measureDc.SetFont(font);
    measureDc.GetTextExtent(label, &textWidth, &textHeight);
  }
  if (textWidth <= 0 || textHeight <= 0)
    return;

  const int padding = 12;
  const int bmpWidth = textWidth + padding * 2;
  const int bmpHeight = textHeight + padding * 2;
  wxBitmap bitmap(bmpWidth, bmpHeight, 32);
  {
    wxMemoryDC dc(bitmap);
    dc.SetBackground(wxBrush(wxColour(0, 0, 0)));
    dc.Clear();
    dc.SetFont(font);
    dc.SetTextForeground(wxColour(255, 255, 255));
    dc.DrawText(label, padding, padding);
    dc.SelectObject(wxNullBitmap);
  }

  wxImage image = bitmap.ConvertToImage();
  if (!image.IsOk())
    return;

  const unsigned char *rgb = image.GetData();
  if (!rgb)
    return;

  std::vector<unsigned char> pixels;
  pixels.resize(static_cast<size_t>(bmpWidth) * bmpHeight * 4);
  for (int i = 0; i < bmpWidth * bmpHeight; ++i) {
    const unsigned char intensity = rgb[i * 3];
    pixels[static_cast<size_t>(i) * 4] = 255;
    pixels[static_cast<size_t>(i) * 4 + 1] = 255;
    pixels[static_cast<size_t>(i) * 4 + 2] = 255;
    pixels[static_cast<size_t>(i) * 4 + 3] = intensity;
  }

  if (!InitGL())
    return;
  glGenTextures(1, &loadingTextTexture_);
  glBindTexture(GL_TEXTURE_2D, loadingTextTexture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bmpWidth, bmpHeight, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixels.data());
  loadingTextTextureSize_ = wxSize(bmpWidth, bmpHeight);
}

void LayoutViewerPanel::ClearLoadingTextTexture() {
  if (loadingTextTexture_ == 0 || !glContext_)
    return;
  if (!IsShown()) {
    loadingTextTexture_ = 0;
    loadingTextTextureSize_ = wxSize(0, 0);
    return;
  }
  SetCurrent(*glContext_);
  glDeleteTextures(1, &loadingTextTexture_);
  loadingTextTexture_ = 0;
  loadingTextTextureSize_ = wxSize(0, 0);
}

void LayoutViewerPanel::DrawSelectionHandles(const wxRect &frameRect) const {
  wxRect handleRight(frameRect.GetRight() - kHandleHalfPx,
                     frameRect.GetTop() + frameRect.GetHeight() / 2 -
                         kHandleHalfPx,
                     kHandleSizePx, kHandleSizePx);
  wxRect handleBottom(frameRect.GetLeft() + frameRect.GetWidth() / 2 -
                          kHandleHalfPx,
                      frameRect.GetBottom() - kHandleHalfPx, kHandleSizePx,
                      kHandleSizePx);
  wxRect handleCorner(frameRect.GetRight() - kHandleHalfPx,
                      frameRect.GetBottom() - kHandleHalfPx, kHandleSizePx,
                      kHandleSizePx);

  glColor4ub(60, 160, 240, 255);
  auto drawHandle = [](const wxRect &rect) {
    glBegin(GL_QUADS);
    glVertex2f(static_cast<float>(rect.GetLeft()),
               static_cast<float>(rect.GetTop()));
    glVertex2f(static_cast<float>(rect.GetRight()),
               static_cast<float>(rect.GetTop()));
    glVertex2f(static_cast<float>(rect.GetRight()),
               static_cast<float>(rect.GetBottom()));
    glVertex2f(static_cast<float>(rect.GetLeft()),
               static_cast<float>(rect.GetBottom()));
    glEnd();
  };
  drawHandle(handleRight);
  drawHandle(handleBottom);
  drawHandle(handleCorner);
}

void LayoutViewerPanel::OnSize(wxSizeEvent &) {
  wxSize size = GetLogicalClientSize(this);
  if (pendingFitOnResize && size.GetWidth() > 0 && size.GetHeight() > 0) {
    ResetViewToFit();
    pendingFitOnResize = false;
  } else {
    InvalidateRenderIfFrameChanged();
  }
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::OnLeftDown(wxMouseEvent &event) {
  SetFocus();
  pendingFrameCommit_ = false;
  deferredResizeFrame_.reset();
  const wxPoint pos = GetLogicalMousePosition(event);
  SelectElementAtPosition(pos);
  layouts::Layout2DViewFrame selectedFrame;
  wxRect frameRect;
  if (GetSelectedFrame(selectedFrame) &&
      GetFrameRect(selectedFrame, frameRect)) {
    FrameDragMode mode = HitTestFrame(pos, frameRect);
    if (mode != FrameDragMode::None) {
      dragMode = mode;
      dragStartPos = pos;
      dragStartFrame = selectedFrame;
      CaptureMouse();
      if (!currentLayout.name.empty()) {
        auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
        cfg.PushUndoState("edit layout element frame");
        layouts::LayoutManager::Get().BeginBatchUpdate();
      }
      return;
    }
  }

  isPanning = true;
  lastMousePos = pos;
  CaptureMouse();
}

void LayoutViewerPanel::OnLeftUp(wxMouseEvent &) {
  if (dragMode != FrameDragMode::None) {
    if (dragMode != FrameDragMode::Move && deferredResizeFrame_.has_value()) {
      layouts::Layout2DViewFrame finalFrame = *deferredResizeFrame_;
      finalFrame.width = std::max(kMinFrameSize, SnapToGrid(finalFrame.width));
      finalFrame.height =
          std::max(kMinFrameSize, SnapToGrid(finalFrame.height));
      ApplyFrameUpdateToSelection(finalFrame, false);
    }
    if (dragMode == FrameDragMode::Move) {
      layouts::Layout2DViewFrame finalFrame;
      if (GetSelectedFrame(finalFrame)) {
        finalFrame.x = SnapToGrid(finalFrame.x);
        finalFrame.y = SnapToGrid(finalFrame.y);
        ApplyFrameUpdateToSelection(finalFrame, true);
      }
      CommitPendingFrameUpdate();
    }
    deferredResizeFrame_.reset();
    dragMode = FrameDragMode::None;
    layouts::LayoutManager::Get().EndBatchUpdate();
    if (HasCapture())
      ReleaseMouse();
    return;
  }
  if (isPanning) {
    isPanning = false;
    if (HasCapture())
      ReleaseMouse();
  }
}

void LayoutViewerPanel::OnLeftDClick(wxMouseEvent &event) {
  const wxPoint pos = GetLogicalMousePosition(event);
  SelectElementAtPosition(pos);
  layouts::Layout2DViewFrame selectedFrame;
  wxRect frameRect;
  if (GetSelectedFrame(selectedFrame) &&
      GetFrameRect(selectedFrame, frameRect) && frameRect.Contains(pos)) {
    if (selectedElementType == SelectedElementType::View2D) {
      EmitEditViewRequest();
      return;
    }
    if (selectedElementType == SelectedElementType::EventTable) {
      wxCommandEvent editEvent;
      OnEditEventTable(editEvent);
      return;
    }
    if (selectedElementType == SelectedElementType::Legend) {
      wxCommandEvent editEvent;
      OnEditLegend(editEvent);
      return;
    }
    if (selectedElementType == SelectedElementType::Text) {
      wxCommandEvent editEvent;
      OnEditText(editEvent);
      return;
    }
    if (selectedElementType == SelectedElementType::Image) {
      wxCommandEvent editEvent;
      OnEditImage(editEvent);
      return;
    }
  }
  event.Skip();
}

bool LayoutViewerPanel::DeleteSelectedElement() {
  wxCommandEvent deleteEvent;
  if (selectedElementType == SelectedElementType::View2D) {
    OnDeleteView(deleteEvent);
    return true;
  }
  if (selectedElementType == SelectedElementType::Legend) {
    OnDeleteLegend(deleteEvent);
    return true;
  }
  if (selectedElementType == SelectedElementType::EventTable) {
    OnDeleteEventTable(deleteEvent);
    return true;
  }
  if (selectedElementType == SelectedElementType::Text) {
    OnDeleteText(deleteEvent);
    return true;
  }
  if (selectedElementType == SelectedElementType::Image) {
    OnDeleteImage(deleteEvent);
    return true;
  }
  return false;
}

void LayoutViewerPanel::OnKeyDown(wxKeyEvent &event) {
  if (gui::IsEditableWidgetFocused(wxWindow::FindFocus())) {
    event.Skip();
    return;
  }

  const int key = event.GetKeyCode();
  if (key == WXK_DELETE || key == WXK_NUMPAD_DELETE) {
    DeleteSelectedElement();
    return;
  }
  if (key == 'Z' || key == 'z') {
    ResetViewToFit();
    RequestRenderRebuild();
    Refresh();
    return;
  }
  event.Skip();
}

void LayoutViewerPanel::OnShow(wxShowEvent &event) {
  if (event.IsShown()) {
    InitGL();
    if (isReadyToRender_ && NeedsRenderRebuild()) {
      RequestRenderRebuild();
    }
  }
  event.Skip();
}

void LayoutViewerPanel::OnMouseMove(wxMouseEvent &event) {
  wxPoint currentPos = GetLogicalMousePosition(event);
  layouts::Layout2DViewFrame selectedFrame;
  wxRect frameRect;
  if (GetSelectedFrame(selectedFrame) &&
      GetFrameRect(selectedFrame, frameRect)) {
    hoverMode = HitTestFrame(currentPos, frameRect);
    SetCursor(CursorForMode(hoverMode));
  } else {
    hoverMode = FrameDragMode::None;
    SetCursor(wxCursor(wxCURSOR_ARROW));
  }

  if (dragMode != FrameDragMode::None && event.Dragging()) {
    SetCursor(CursorForMode(dragMode));
    wxPoint delta = currentPos - dragStartPos;
    wxPoint logicalDelta(static_cast<int>(std::lround(delta.x / zoom)),
                         static_cast<int>(std::lround(delta.y / zoom)));
    layouts::Layout2DViewFrame frame = dragStartFrame;
    if (dragMode == FrameDragMode::Move) {
      frame.x += logicalDelta.x;
      frame.y += logicalDelta.y;
    } else {
      if (selectedElementType == SelectedElementType::Image) {
        const auto *image = GetSelectedImage();
        const double ratio = image && image->aspectRatio > 0.0f
                                 ? image->aspectRatio
                                 : 0.0;
        const bool useHeight =
            dragMode == FrameDragMode::ResizeBottom ||
            (dragMode == FrameDragMode::ResizeCorner &&
             std::abs(logicalDelta.y) > std::abs(logicalDelta.x));
        if (ratio > 0.0) {
          if (dragMode == FrameDragMode::ResizeRight ||
              dragMode == FrameDragMode::ResizeCorner) {
            frame.width = std::max(
                kMinFrameSize, dragStartFrame.width + logicalDelta.x);
            frame.height = std::max(
                kMinFrameSize,
                static_cast<int>(std::lround(frame.width / ratio)));
          }
          if (dragMode == FrameDragMode::ResizeBottom ||
              dragMode == FrameDragMode::ResizeCorner) {
            const int candidateHeight = std::max(
                kMinFrameSize, dragStartFrame.height + logicalDelta.y);
            const int candidateWidth = std::max(
                kMinFrameSize,
                static_cast<int>(std::lround(candidateHeight * ratio)));
            if (dragMode == FrameDragMode::ResizeBottom ||
                std::abs(logicalDelta.y) > std::abs(logicalDelta.x)) {
              frame.height = candidateHeight;
              frame.width = candidateWidth;
            }
          }
          if (useHeight) {
            frame.height = std::max(kMinFrameSize, frame.height);
            frame.width = std::max(
                kMinFrameSize,
                static_cast<int>(std::lround(frame.height * ratio)));
          } else {
            frame.width = std::max(kMinFrameSize, frame.width);
            frame.height = std::max(
                kMinFrameSize,
                static_cast<int>(std::lround(frame.width / ratio)));
          }
        } else {
          if (dragMode == FrameDragMode::ResizeRight ||
              dragMode == FrameDragMode::ResizeCorner) {
            frame.width =
                std::max(kMinFrameSize, dragStartFrame.width + logicalDelta.x);
          }
          if (dragMode == FrameDragMode::ResizeBottom ||
              dragMode == FrameDragMode::ResizeCorner) {
            frame.height =
                std::max(kMinFrameSize, dragStartFrame.height + logicalDelta.y);
          }
        }
      } else {
        if (dragMode == FrameDragMode::ResizeRight ||
            dragMode == FrameDragMode::ResizeCorner) {
          frame.width =
              std::max(kMinFrameSize, dragStartFrame.width + logicalDelta.x);
        }
        if (dragMode == FrameDragMode::ResizeBottom ||
            dragMode == FrameDragMode::ResizeCorner) {
          frame.height =
              std::max(kMinFrameSize, dragStartFrame.height + logicalDelta.y);
        }
      }
    }
    const bool updatePosition = dragMode == FrameDragMode::Move;
    if (updatePosition) {
      ApplyFrameUpdateToSelection(frame, true);
    } else {
      deferredResizeFrame_ = frame;
      Refresh();
    }
    return;
  }

  if (!isPanning || !event.Dragging())
    return;

  wxPoint delta = currentPos - lastMousePos;
  panOffset += delta;
  lastMousePos = currentPos;
  Refresh();
}

void LayoutViewerPanel::OnMouseWheel(wxMouseEvent &event) {
  if (dragMode != FrameDragMode::None)
    return;
  const int rotation = event.GetWheelRotation();
  const int delta = event.GetWheelDelta();
  if (delta == 0 || rotation == 0)
    return;

  const double steps = static_cast<double>(rotation) /
                       static_cast<double>(delta);
  const double factor = std::pow(kZoomStep, steps);
  const double safeMaxZoom = GetLayoutSafeMaxZoom(currentLayout);
  const double newZoom = std::clamp(zoom * factor, kMinZoom, safeMaxZoom);
  if (std::abs(newZoom - zoom) < 1e-6)
    return;

  wxSize size = GetLogicalClientSize(this);
  wxPoint center(size.GetWidth() / 2, size.GetHeight() / 2);
  wxPoint mousePos = GetLogicalMousePosition(event);

  wxPoint relative = mousePos - center - panOffset;
  const double scale = newZoom / zoom;
  wxPoint newRelative(static_cast<int>(relative.x * scale),
                      static_cast<int>(relative.y * scale));

  panOffset += relative - newRelative;
  zoom = newZoom;
  InvalidateRenderIfFrameChanged();
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::OnCaptureLost(wxMouseCaptureLostEvent &) {
  isPanning = false;
  deferredResizeFrame_.reset();
  CommitPendingFrameUpdate();
  if (dragMode != FrameDragMode::None) {
    layouts::LayoutManager::Get().EndBatchUpdate();
  }
  dragMode = FrameDragMode::None;
}

void LayoutViewerPanel::ApplyFrameUpdateToSelection(
    const layouts::Layout2DViewFrame &frame, bool updatePosition) {
  if (selectedElementType == SelectedElementType::Legend) {
    UpdateLegendFrame(frame, updatePosition);
  } else if (selectedElementType == SelectedElementType::EventTable) {
    UpdateEventTableFrame(frame, updatePosition);
  } else if (selectedElementType == SelectedElementType::Text) {
    UpdateTextFrame(frame, updatePosition);
  } else if (selectedElementType == SelectedElementType::Image) {
    UpdateImageFrame(frame, updatePosition);
  } else {
    UpdateFrame(frame, updatePosition);
  }
}

void LayoutViewerPanel::CommitPendingFrameUpdate() {
  if (!pendingFrameCommit_)
    return;
  pendingFrameCommit_ = false;
  if (currentLayout.name.empty())
    return;

  if (selectedElementType == SelectedElementType::View2D) {
    if (const auto *view = GetEditableView()) {
      layouts::LayoutManager::Get().UpdateLayout2DView(currentLayout.name,
                                                       *view);
    }
    return;
  }

  if (selectedElementType == SelectedElementType::Legend) {
    if (const auto *legend = GetSelectedLegend()) {
      layouts::LayoutManager::Get().UpdateLayoutLegend(currentLayout.name,
                                                       *legend);
    }
    return;
  }

  if (selectedElementType == SelectedElementType::EventTable) {
    if (const auto *table = GetSelectedEventTable()) {
      layouts::LayoutManager::Get().UpdateLayoutEventTable(currentLayout.name,
                                                           *table);
    }
    return;
  }

  if (selectedElementType == SelectedElementType::Text) {
    if (const auto *text = GetSelectedText()) {
      layouts::LayoutManager::Get().UpdateLayoutText(currentLayout.name, *text);
    }
    return;
  }

  if (selectedElementType == SelectedElementType::Image) {
    if (const auto *image = GetSelectedImage()) {
      layouts::LayoutManager::Get().UpdateLayoutImage(currentLayout.name,
                                                      *image);
    }
  }
}

void LayoutViewerPanel::OnRightUp(wxMouseEvent &event) {
  SetFocus();
  const wxPoint pos = GetLogicalMousePosition(event);
  if (!SelectElementAtPosition(pos)) {
    event.Skip();
    return;
  }
  layouts::Layout2DViewFrame selectedFrame;
  wxRect frameRect;
  if (!(GetSelectedFrame(selectedFrame) &&
        GetFrameRect(selectedFrame, frameRect) && frameRect.Contains(pos))) {
    event.Skip();
    return;
  }

  wxMenu menu;
  if (selectedElementType == SelectedElementType::View2D) {
    menu.Append(kEditMenuId, "2D View Editor");
    menu.AppendCheckItem(kToggleViewFrameMenuId, "Show Border");
    menu.Append(kDeleteMenuId, "Delete 2D View");
    if (const auto *view = GetEditableView())
      menu.Check(kToggleViewFrameMenuId, view->drawFrame);
    menu.AppendSeparator();
    menu.Append(kBringToFrontMenuId, "Bring to Front");
    menu.Append(kSendToBackMenuId, "Send to Back");
  } else if (selectedElementType == SelectedElementType::Legend) {
    menu.Append(kEditLegendMenuId, "Edit Legend");
    menu.Append(kDeleteLegendMenuId, "Delete Legend");
    menu.AppendSeparator();
    menu.Append(kBringToFrontMenuId, "Bring to Front");
    menu.Append(kSendToBackMenuId, "Send to Back");
  } else if (selectedElementType == SelectedElementType::EventTable) {
    menu.Append(kEditEventTableMenuId, "Edit Event Table");
    menu.Append(kDeleteEventTableMenuId, "Delete Event Table");
    menu.AppendSeparator();
    menu.Append(kBringToFrontMenuId, "Bring to Front");
    menu.Append(kSendToBackMenuId, "Send to Back");
  } else if (selectedElementType == SelectedElementType::Text) {
    menu.Append(kEditTextMenuId, "Edit Text");
    menu.AppendCheckItem(kToggleTextFrameMenuId, "Show Border");
    menu.AppendCheckItem(kToggleTextTransparentBackgroundMenuId,
                         "Transparent Background");
    menu.Append(kDeleteTextMenuId, "Delete Text");
    if (const auto *text = GetSelectedText()) {
      menu.Check(kToggleTextFrameMenuId, text->drawFrame);
      menu.Check(kToggleTextTransparentBackgroundMenuId,
                 !text->solidBackground);
    }
    menu.AppendSeparator();
    menu.Append(kBringToFrontMenuId, "Bring to Front");
    menu.Append(kSendToBackMenuId, "Send to Back");
  } else if (selectedElementType == SelectedElementType::Image) {
    menu.Append(kEditImageMenuId, "Change Image");
    menu.Append(kDeleteImageMenuId, "Delete Image");
    menu.AppendSeparator();
    menu.Append(kBringToFrontMenuId, "Bring to Front");
    menu.Append(kSendToBackMenuId, "Send to Back");
  }
  PopupMenu(&menu, pos);
}

void LayoutViewerPanel::OnBringToFront(wxCommandEvent &) {
  if (selectedElementId < 0)
    return;
  const int maxZ = GetZIndexRange().second;
  if (selectedElementType == SelectedElementType::View2D) {
    auto it =
        std::find_if(currentLayout.view2dViews.begin(),
                     currentLayout.view2dViews.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.view2dViews.end())
      return;
    it->zIndex = maxZ + 1;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("bring layout element to front");
      layouts::LayoutManager::Get().UpdateLayout2DView(currentLayout.name, *it);
    }
  } else if (selectedElementType == SelectedElementType::Legend) {
    auto it =
        std::find_if(currentLayout.legendViews.begin(),
                     currentLayout.legendViews.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.legendViews.end())
      return;
    it->zIndex = maxZ + 1;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("bring layout element to front");
      layouts::LayoutManager::Get().UpdateLayoutLegend(currentLayout.name,
                                                       *it);
    }
  } else if (selectedElementType == SelectedElementType::EventTable) {
    auto it =
        std::find_if(currentLayout.eventTables.begin(),
                     currentLayout.eventTables.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.eventTables.end())
      return;
    it->zIndex = maxZ + 1;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("bring layout element to front");
      layouts::LayoutManager::Get().UpdateLayoutEventTable(currentLayout.name,
                                                           *it);
    }
  } else if (selectedElementType == SelectedElementType::Text) {
    auto it =
        std::find_if(currentLayout.textViews.begin(),
                     currentLayout.textViews.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.textViews.end())
      return;
    it->zIndex = maxZ + 1;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("bring layout element to front");
      layouts::LayoutManager::Get().UpdateLayoutText(currentLayout.name, *it);
    }
  } else if (selectedElementType == SelectedElementType::Image) {
    auto it =
        std::find_if(currentLayout.imageViews.begin(),
                     currentLayout.imageViews.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.imageViews.end())
      return;
    it->zIndex = maxZ + 1;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("bring layout element to front");
      layouts::LayoutManager::Get().UpdateLayoutImage(currentLayout.name, *it);
    }
  } else {
    return;
  }
  layoutVersion++;
  InvalidateSelectionIndexCache();
  contentDirty = true;
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::OnSendToBack(wxCommandEvent &) {
  if (selectedElementId < 0)
    return;
  const int minZ = GetZIndexRange().first;
  if (selectedElementType == SelectedElementType::View2D) {
    auto it =
        std::find_if(currentLayout.view2dViews.begin(),
                     currentLayout.view2dViews.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.view2dViews.end())
      return;
    it->zIndex = minZ - 1;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("send layout element to back");
      layouts::LayoutManager::Get().UpdateLayout2DView(currentLayout.name, *it);
    }
  } else if (selectedElementType == SelectedElementType::Legend) {
    auto it =
        std::find_if(currentLayout.legendViews.begin(),
                     currentLayout.legendViews.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.legendViews.end())
      return;
    it->zIndex = minZ - 1;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("send layout element to back");
      layouts::LayoutManager::Get().UpdateLayoutLegend(currentLayout.name,
                                                       *it);
    }
  } else if (selectedElementType == SelectedElementType::EventTable) {
    auto it =
        std::find_if(currentLayout.eventTables.begin(),
                     currentLayout.eventTables.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.eventTables.end())
      return;
    it->zIndex = minZ - 1;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("send layout element to back");
      layouts::LayoutManager::Get().UpdateLayoutEventTable(currentLayout.name,
                                                           *it);
    }
  } else if (selectedElementType == SelectedElementType::Text) {
    auto it =
        std::find_if(currentLayout.textViews.begin(),
                     currentLayout.textViews.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.textViews.end())
      return;
    it->zIndex = minZ - 1;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("send layout element to back");
      layouts::LayoutManager::Get().UpdateLayoutText(currentLayout.name, *it);
    }
  } else if (selectedElementType == SelectedElementType::Image) {
    auto it =
        std::find_if(currentLayout.imageViews.begin(),
                     currentLayout.imageViews.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.imageViews.end())
      return;
    it->zIndex = minZ - 1;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("send layout element to back");
      layouts::LayoutManager::Get().UpdateLayoutImage(currentLayout.name, *it);
    }
  } else {
    return;
  }
  layoutVersion++;
  InvalidateSelectionIndexCache();
  contentDirty = true;
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::ResetViewToFit() {
  wxSize size = GetLogicalClientSize(this);
  const double pageWidth = currentLayout.pageSetup.PageWidthPt();
  const double pageHeight = currentLayout.pageSetup.PageHeightPt();

  if (pageWidth <= 0.0 || pageHeight <= 0.0 || size.GetWidth() <= 0 ||
      size.GetHeight() <= 0) {
    zoom = 1.0;
    panOffset = wxPoint(0, 0);
    return;
  }

  const double fitWidth =
      static_cast<double>(size.GetWidth() - kFitMarginPx) / pageWidth;
  const double fitHeight =
      static_cast<double>(size.GetHeight() - kFitMarginPx) / pageHeight;
  zoom = std::clamp(std::min(fitWidth, fitHeight), kMinZoom, kMaxZoom);
  panOffset = wxPoint(0, 0);
  InvalidateRenderIfFrameChanged();
}

wxRect LayoutViewerPanel::GetPageRect() const {
  wxSize size = GetLogicalClientSize(this);
  const double pageWidth = currentLayout.pageSetup.PageWidthPt();
  const double pageHeight = currentLayout.pageSetup.PageHeightPt();
  const double scaledWidth = pageWidth * zoom;
  const double scaledHeight = pageHeight * zoom;
  const wxPoint center(size.GetWidth() / 2, size.GetHeight() / 2);
  const wxPoint topLeft(center.x - static_cast<int>(scaledWidth / 2.0) +
                            panOffset.x,
                        center.y - static_cast<int>(scaledHeight / 2.0) +
                            panOffset.y);
  return wxRect(topLeft.x, topLeft.y, static_cast<int>(scaledWidth),
                static_cast<int>(scaledHeight));
}

bool LayoutViewerPanel::GetFrameRect(const layouts::Layout2DViewFrame &frame,
                                     wxRect &rect) const {
  if (frame.width <= 0 || frame.height <= 0)
    return false;
  wxRect pageRect = GetPageRect();
  const int scaledX = static_cast<int>(std::lround(frame.x * zoom));
  const int scaledY = static_cast<int>(std::lround(frame.y * zoom));
  const int scaledWidth = static_cast<int>(std::lround(frame.width * zoom));
  const int scaledHeight = static_cast<int>(std::lround(frame.height * zoom));
  rect = wxRect(pageRect.GetLeft() + scaledX, pageRect.GetTop() + scaledY,
                scaledWidth, scaledHeight);
  return true;
}

wxSize LayoutViewerPanel::GetFrameSizeForZoom(
    const layouts::Layout2DViewFrame &frame, double targetZoom) const {
  if (frame.width <= 0 || frame.height <= 0 || targetZoom <= 0.0)
    return wxSize(0, 0);
  const double scaledWidthValue = frame.width * targetZoom;
  const double scaledHeightValue = frame.height * targetZoom;
  if (scaledWidthValue > kMaxRenderDimension ||
      scaledHeightValue > kMaxRenderDimension)
    return wxSize(0, 0);
  const int scaledWidth = static_cast<int>(std::lround(scaledWidthValue));
  const int scaledHeight = static_cast<int>(std::lround(scaledHeightValue));
  if (scaledWidth <= 0 || scaledHeight <= 0)
    return wxSize(0, 0);
  if (scaledWidth > kMaxRenderDimension ||
      scaledHeight > kMaxRenderDimension)
    return wxSize(0, 0);
  if (static_cast<size_t>(scaledWidth) >
      kMaxRenderPixels / static_cast<size_t>(scaledHeight))
    return wxSize(0, 0);
  const size_t pixelCount =
      static_cast<size_t>(scaledWidth) * static_cast<size_t>(scaledHeight);
  if (pixelCount > kMaxRenderPixels)
    return wxSize(0, 0);
  if (pixelCount > kMaxRenderBytes / 4)
    return wxSize(0, 0);
  return wxSize(scaledWidth, scaledHeight);
}

double LayoutViewerPanel::GetRenderZoom() const {
  return zoom;
}

bool LayoutViewerPanel::GetSelectedFrame(
    layouts::Layout2DViewFrame &frame) const {
  if (selectedElementType == SelectedElementType::Legend) {
    const auto *legend = GetSelectedLegend();
    if (!legend)
      return false;
    frame = legend->frame;
    return true;
  }
  if (selectedElementType == SelectedElementType::EventTable) {
    const auto *table = GetSelectedEventTable();
    if (!table)
      return false;
    frame = table->frame;
    return true;
  }
  if (selectedElementType == SelectedElementType::Text) {
    const auto *text = GetSelectedText();
    if (!text)
      return false;
    frame = text->frame;
    return true;
  }
  if (selectedElementType == SelectedElementType::Image) {
    const auto *image = GetSelectedImage();
    if (!image)
      return false;
    frame = image->frame;
    return true;
  }
  const auto *view = GetEditableView();
  if (!view)
    return false;
  frame = view->frame;
  return true;
}

// Initializes layout viewer OpenGL resources through centralized GLEW/context validation.
bool LayoutViewerPanel::InitGL() {
  if (!glContext_)
    return false;
  if (!IsShownOnScreen())
    return false;
  if (!glInitialized_) {
    const GLEWInitResult initResult =
        InitializeGlewForCurrentContext(*this, *glContext_, "LayoutViewerPanel");
    if (!initResult.success) {
      isReadyToRender_ = false;
      Logger::Instance().Log(initResult.message);
      return false;
    }
    if (initResult.isWarningOnly) {
      Logger::Instance().Log(initResult.message);
    }
    glInitialized_ = true;
  }

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  isReadyToRender_ = true;
  return true;
}

// Rebuilds cached textures incrementally across dirty stages to keep the UI responsive.
void LayoutViewerPanel::RebuildCachedTexture() {
  try {
    if (!NeedsRenderRebuild())
      return;
    if (!isReadyToRender_ || !glContext_ || !IsShownOnScreen())
      return;

    auto stopLoadingRequest = [this]() {
      loadingRequested = false;
      if (loadingTimer_.IsRunning())
        loadingTimer_.Stop();
    };
    auto clearLoadingState = [this, stopLoadingRequest]() {
      stopLoadingRequest();
      isLoading = false;
    };

    contentDirty = false;
    presentationDirty = false;
    StartRebuildTickBudget();

    bool needsLegendProcessing = false;
    bool needsLegendSymbolCapture = false;
    Viewer2DOffscreenRenderer *offscreenRenderer = nullptr;
    Viewer2DPanel *capturePanel = nullptr;
    if (!EnsureRebuildResourcesReady(needsLegendProcessing, needsLegendSymbolCapture,
                                     offscreenRenderer, capturePanel)) {
      return;
    }

    ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
    std::shared_ptr<const SymbolDefinitionSnapshot> legendSymbols;
    if (!PrepareLegendSymbolsIfNeeded(needsLegendSymbolCapture, capturePanel, cfg,
                                      legendSymbols)) {
      clearLoadingState();
      NotifyRenderReady();
      return;
    }

    std::vector<unsigned char> legendPixels;
    std::vector<unsigned char> eventTablePixels;
    std::vector<unsigned char> textPixels;
    std::vector<unsigned char> imagePixels;
    const double renderZoom = GetRenderZoom();
    constexpr int kMaxItemsPerStage = 3;
    bool timeBudgetExpired = false;

    if (!ProcessDirty2DViews(offscreenRenderer, capturePanel, cfg, renderZoom,
                             kMaxItemsPerStage, timeBudgetExpired) ||
        !ProcessDirtyLegends(renderZoom, kMaxItemsPerStage, legendSymbols,
                             legendPixels, timeBudgetExpired) ||
        !ProcessDirtyEventTables(renderZoom, kMaxItemsPerStage, eventTablePixels,
                                 timeBudgetExpired) ||
        !ProcessDirtyTexts(renderZoom, kMaxItemsPerStage, textPixels,
                           timeBudgetExpired) ||
        !ProcessDirtyImages(renderZoom, kMaxItemsPerStage, imagePixels,
                            timeBudgetExpired)) {
      clearLoadingState();
      NotifyRenderReady();
      return;
    }

    if (NeedsRenderRebuild() || timeBudgetExpired) {
      RequestRenderRebuild();
      NotifyRenderReady();
      return;
    }

    rebuildViewCursor_ = rebuildLegendCursor_ = rebuildEventTableCursor_ = 0;
    rebuildTextCursor_ = rebuildImageCursor_ = 0;
    clearLoadingState();
    NotifyRenderReady();
  } catch (const std::exception &ex) {
    loadingRequested = false;
    isLoading = false;
    Logger::Instance().Log(
        std::string("LayoutViewerPanel::RebuildCachedTexture exception: ") +
        ex.what());
    NotifyRenderReady();
  } catch (...) {
    loadingRequested = false;
    isLoading = false;
    Logger::Instance().Log(
        "LayoutViewerPanel::RebuildCachedTexture unknown exception.");
    NotifyRenderReady();
  }
}


// Starts a new per-tick deadline used by progressive texture rebuilding.
void LayoutViewerPanel::StartRebuildTickBudget() { rebuildTickStart_ = std::chrono::steady_clock::now(); }

// Reports whether the current progressive rebuild tick exhausted its time budget.
bool LayoutViewerPanel::IsRebuildTimeBudgetExpired() const { return std::chrono::steady_clock::now() - rebuildTickStart_ >= rebuildTickBudget_; }

// Logs per-stage rebuild metrics for visibility into expensive texture categories.
void LayoutViewerPanel::LogRebuildStageMetrics(const char *stageName, int processedCount, std::chrono::steady_clock::duration elapsed) const {
  const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
  if (processedCount == 0 && elapsedMs == 0)
    return;
  Logger::Instance().Log(std::string("LayoutViewerPanel rebuild stage ") + stageName + ": count=" + std::to_string(processedCount) + ", ms=" + std::to_string(elapsedMs));
}

// Collects rebuild prerequisites and lazily resolves offscreen rendering dependencies.
bool LayoutViewerPanel::EnsureRebuildResourcesReady(bool &needsLegendProcessing, bool &needsLegendSymbolCapture, Viewer2DOffscreenRenderer *&offscreenRenderer, Viewer2DPanel *&capturePanel) {
  bool hasDirtyViewCache = false;
  for (const auto &view : currentLayout.view2dViews) {
    const auto cacheIt = viewCaches_.find(view.id);
    if (cacheIt != viewCaches_.end() && cacheIt->second.renderDirty) { hasDirtyViewCache = true; break; }
  }
  needsLegendProcessing = false; needsLegendSymbolCapture = false;
  for (const auto &legend : currentLayout.legendViews) {
    const auto cacheIt = legendCaches_.find(legend.id);
    const bool cacheMissing = cacheIt == legendCaches_.end();
    const bool contentChanged = !cacheMissing && cacheIt->second.contentHash != legendDataHash;
    const bool needsTextureRebuild = cacheMissing || cacheIt->second.renderDirty || contentChanged;
    if (needsTextureRebuild) { needsLegendProcessing = true; const bool needsSymbols = cacheMissing || !cacheIt->second.symbols || contentChanged; if (needsSymbols) needsLegendSymbolCapture = true; }
  }
  const bool needsCapturePanel = hasDirtyViewCache || needsLegendSymbolCapture;
  if (!needsCapturePanel) return true;
  if (auto *mw = MainWindow::Instance()) { offscreenRenderer = mw->GetOffscreenRenderer(); capturePanel = offscreenRenderer ? offscreenRenderer->GetPanel() : nullptr; }
  return capturePanel && offscreenRenderer;
}

// Captures legend symbol snapshots only when required by dirty legend caches.
bool LayoutViewerPanel::PrepareLegendSymbolsIfNeeded(bool needsLegendSymbolCapture, Viewer2DPanel *capturePanel, ConfigManager &cfg, std::shared_ptr<const SymbolDefinitionSnapshot> &legendSymbols) {
  if (!needsLegendSymbolCapture) return true;
  if (!capturePanel) return false;
  legendSymbols = CaptureLegendSymbolSnapshot(capturePanel, cfg, true);
  return true;
}

// Processes dirty 2D view textures in bounded batches and uploads each successful render to GPU.
bool LayoutViewerPanel::ProcessDirty2DViews(Viewer2DOffscreenRenderer *offscreenRenderer, Viewer2DPanel *capturePanel, ConfigManager &cfg, double renderZoom, int maxItems, bool &timeBudgetExpired) {
  if (!offscreenRenderer || !capturePanel)
    return true;
  const auto stageStart = std::chrono::steady_clock::now();
  int processedCount = 0;
  if (currentLayout.view2dViews.empty())
    return true;
  const size_t total = currentLayout.view2dViews.size();
  for (int pass = 0; pass < 2; ++pass) {
    for (size_t scanned = 0; scanned < total; ++scanned) {
      const size_t idx = (rebuildViewCursor_ + scanned) % total;
      const auto &view = currentLayout.view2dViews[idx];
      wxRect frameRect;
      const bool isVisible = GetFrameRect(view.frame, frameRect) && frameRect.Intersects(wxRect(wxPoint(0, 0), GetClientSize()));
      const bool isSelected = selectedElementType == SelectedElementType::View2D && selectedElementId == view.id;
      const bool isPriority = isVisible || isSelected;
      if ((pass == 0 && !isPriority) || (pass == 1 && isPriority))
        continue;
    ViewCache &cache = GetViewCache(view.id);
    if (!cache.renderDirty)
      continue;
    cache.renderDirty = false;
    if (!cache.hasCapture || !cache.hasRenderState || !GetFrameRect(view.frame, frameRect)) {
      ClearCachedTexture(cache);
      cache.textureSize = wxSize(0, 0);
      cache.renderZoom = 0.0;
    } else {
      const wxSize renderSize = GetFrameSizeForZoom(view.frame, renderZoom);
      if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
      } else {
        offscreenRenderer->SetViewportSize(renderSize);
        offscreenRenderer->PrepareForCapture();
        viewer2d::Viewer2DState renderState = cache.renderState;
        if (renderZoom != 1.0)
          renderState.camera.zoom *= static_cast<float>(renderZoom);
        renderState.camera.viewportWidth = renderSize.GetWidth();
        renderState.camera.viewportHeight = renderSize.GetHeight();
        auto stateGuard = std::make_shared<viewer2d::ScopedViewer2DState>(
            capturePanel, nullptr, cfg, renderState, capturePanel, nullptr,
            false);
        std::vector<unsigned char> pixels;
        int width = 0;
        int height = 0;
        capturePanel->SetPreferPerastageSvgSymbolsForLayouts(true);
        const bool rendered = capturePanel->RenderToRGBA(pixels, width, height);
        capturePanel->SetPreferPerastageSvgSymbolsForLayouts(false);
        if (!rendered || width <= 0 || height <= 0) {
          ClearCachedTexture(cache);
          cache.textureSize = wxSize(0, 0);
          cache.renderZoom = 0.0;
        } else {
          if (!InitGL())
            return false;
          if (cache.texture == 0)
            glGenTextures(1, &cache.texture);
          glBindTexture(GL_TEXTURE_2D, cache.texture);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
          glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
          ScopedActivePixelUnpackPbo scopedPbo(cache.pixelUnpackPbo, cache.pboBytes);
          if (!UploadRgbaToTexture(cache.texture, width, height, pixels.data(), cache.textureSize, true)) {
            ClearCachedTexture(cache);
            cache.textureSize = wxSize(0, 0);
            cache.renderZoom = 0.0;
          } else {
            cache.textureSize = wxSize(width, height);
            cache.renderZoom = renderZoom;
            cache.contentHash = HashViewContent(view);
          }
        }
      }
    }
      processedCount++;
      rebuildViewCursor_ = (idx + 1) % total;
      if (processedCount >= maxItems || IsRebuildTimeBudgetExpired()) {
        timeBudgetExpired = true;
        break;
      }
    }
    if (timeBudgetExpired)
      break;
  }
  LogRebuildStageMetrics("2d_views", processedCount, std::chrono::steady_clock::now() - stageStart);
  return true;
}

// Processes dirty legend textures in bounded batches and refreshes symbol snapshots when needed.
bool LayoutViewerPanel::ProcessDirtyLegends(double renderZoom, int maxItems, const std::shared_ptr<const SymbolDefinitionSnapshot> &legendSymbols, std::vector<unsigned char> &legendPixels, bool &timeBudgetExpired) {
  const auto stageStart = std::chrono::steady_clock::now();
  int processedCount = 0;
  if (currentLayout.legendViews.empty())
    return true;
  const size_t total = currentLayout.legendViews.size();
  for (int pass = 0; pass < 2; ++pass) {
    for (size_t scanned = 0; scanned < total; ++scanned) {
      const size_t idx = (rebuildLegendCursor_ + scanned) % total;
      const auto &legend = currentLayout.legendViews[idx];
      wxRect frameRect;
      const bool isVisible = GetFrameRect(legend.frame, frameRect) && frameRect.Intersects(wxRect(wxPoint(0, 0), GetClientSize()));
      const bool isSelected = selectedElementType == SelectedElementType::Legend && selectedElementId == legend.id;
      const bool isPriority = isVisible || isSelected;
      if ((pass == 0 && !isPriority) || (pass == 1 && isPriority))
        continue;
    LegendCache &cache = GetLegendCache(legend.id);
    const bool contentChanged = cache.contentHash != legendDataHash;
    const bool requiresSymbolRefresh = !cache.symbols || contentChanged;
    if (requiresSymbolRefresh && legendSymbols && cache.symbols != legendSymbols) { cache.symbols = legendSymbols; cache.renderDirty = true; }
    if (contentChanged) cache.renderDirty = true;
    if (!cache.renderDirty) continue;
    cache.renderDirty = false;
    const wxSize renderSize = GetFrameSizeForZoom(legend.frame, renderZoom);
    if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0) { ClearCachedTexture(cache); cache.textureSize = wxSize(0, 0); cache.renderZoom = 0.0; }
    else {
      wxImage image = BuildLegendImage(renderSize, wxSize(legend.frame.width, legend.frame.height), renderZoom, legendItems_, cache.symbols.get());
      if (!image.IsOk()) { ClearCachedTexture(cache); cache.textureSize = wxSize(0, 0); cache.renderZoom = 0.0; }
      else {
        image = image.Mirror(false); if (!image.HasAlpha()) image.InitAlpha();
        const int width = image.GetWidth(), height = image.GetHeight(); const unsigned char *rgb = image.GetData(); const unsigned char *alpha = image.GetAlpha();
        if (!rgb || width <= 0 || height <= 0 || !TryAllocatePixelBuffer(legendPixels, width, height, "legend")) { ClearCachedTexture(cache); cache.textureSize = wxSize(0, 0); cache.renderZoom = 0.0; }
        else {
          for (int i = 0; i < width * height; ++i) { legendPixels[static_cast<size_t>(i) * 4] = rgb[i * 3]; legendPixels[static_cast<size_t>(i) * 4 + 1] = rgb[i * 3 + 1]; legendPixels[static_cast<size_t>(i) * 4 + 2] = rgb[i * 3 + 2]; legendPixels[static_cast<size_t>(i) * 4 + 3] = alpha ? alpha[i] : 255; }
          if (!InitGL()) return false;
          if (cache.texture == 0) glGenTextures(1, &cache.texture);
          glBindTexture(GL_TEXTURE_2D, cache.texture); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
          ScopedActivePixelUnpackPbo scopedPbo(cache.pixelUnpackPbo, cache.pboBytes);
          if (!UploadRgbaToTexture(cache.texture, width, height, legendPixels.data(), cache.textureSize, true)) { ClearCachedTexture(cache); cache.textureSize = wxSize(0, 0); cache.renderZoom = 0.0; }
          else { cache.textureSize = wxSize(width, height); cache.renderZoom = renderZoom; cache.contentHash = legendDataHash; }
          legendPixels.clear();
        }
      }
    }
      processedCount++; rebuildLegendCursor_ = (idx + 1) % total;
      if (processedCount >= maxItems || IsRebuildTimeBudgetExpired()) { timeBudgetExpired = true; break; }
    }
    if (timeBudgetExpired)
      break;
  }
  LogRebuildStageMetrics("legends", processedCount, std::chrono::steady_clock::now() - stageStart);
  return true;
}

// Processes dirty event table textures in bounded batches to progressively refresh overlay tables.
bool LayoutViewerPanel::ProcessDirtyEventTables(double renderZoom, int maxItems, std::vector<unsigned char> &eventTablePixels, bool &timeBudgetExpired) {
  const auto stageStart = std::chrono::steady_clock::now();
  int processedCount = 0;
  const size_t total = currentLayout.eventTables.size();
  for (int pass = 0; pass < 2; ++pass) {
    for (size_t scanned = 0; scanned < total; ++scanned) {
      const size_t idx = (rebuildEventTableCursor_ + scanned) % total;
      const auto &table = currentLayout.eventTables[idx];
      wxRect frameRect;
      const bool isVisible = GetFrameRect(table.frame, frameRect) && frameRect.Intersects(wxRect(wxPoint(0, 0), GetClientSize()));
      const bool isSelected = selectedElementType == SelectedElementType::EventTable && selectedElementId == table.id;
      const bool isPriority = isVisible || isSelected;
      if ((pass == 0 && !isPriority) || (pass == 1 && isPriority))
        continue;
    EventTableCache &cache = GetEventTableCache(table.id);
    size_t dataHash = HashEventTableFields(table);
    if (cache.contentHash != dataHash) cache.renderDirty = true;
    if (!cache.renderDirty) continue;
    cache.renderDirty = false;
    const wxSize renderSize = GetFrameSizeForZoom(table.frame, renderZoom);
    if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0) { ClearCachedTexture(cache); cache.textureSize = wxSize(0,0); cache.renderZoom = 0.0; }
    else {
      wxImage image = BuildEventTableImage(renderSize, wxSize(table.frame.width, table.frame.height), renderZoom, table);
      if (!image.IsOk()) { ClearCachedTexture(cache); cache.textureSize = wxSize(0,0); cache.renderZoom = 0.0; }
      else {
        image = image.Mirror(false); if (!image.HasAlpha()) image.InitAlpha();
        const int width=image.GetWidth(), height=image.GetHeight(); const unsigned char *rgb=image.GetData(), *alpha=image.GetAlpha();
        if (!rgb || width<=0 || height<=0 || !TryAllocatePixelBuffer(eventTablePixels,width,height,"event table")) { ClearCachedTexture(cache); cache.textureSize = wxSize(0,0); cache.renderZoom = 0.0; }
        else {
          for (int i=0;i<width*height;++i){ const unsigned char a=alpha?alpha[i]:255; eventTablePixels[(size_t)i*4]=rgb[i*3]; eventTablePixels[(size_t)i*4+1]=rgb[i*3+1]; eventTablePixels[(size_t)i*4+2]=rgb[i*3+2]; eventTablePixels[(size_t)i*4+3]=a; }
          if (!InitGL()) return false;
          if (cache.texture==0) glGenTextures(1,&cache.texture);
          glBindTexture(GL_TEXTURE_2D, cache.texture); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
          ScopedActivePixelUnpackPbo scopedPbo(cache.pixelUnpackPbo, cache.pboBytes);
          if (!UploadRgbaToTexture(cache.texture,width,height,eventTablePixels.data(),cache.textureSize,true)) { ClearCachedTexture(cache); cache.textureSize = wxSize(0,0); cache.renderZoom = 0.0; }
          else { cache.textureSize = wxSize(width,height); cache.renderZoom = renderZoom; cache.contentHash = dataHash; }
          eventTablePixels.clear();
        }
      }
    }
      processedCount++; rebuildEventTableCursor_=(idx+1)%total;
      if (processedCount>=maxItems || IsRebuildTimeBudgetExpired()) { timeBudgetExpired=true; break; }
    }
    if (timeBudgetExpired)
      break;
  }
  LogRebuildStageMetrics("event_tables", processedCount, std::chrono::steady_clock::now()-stageStart);
  return true;
}

// Processes dirty text textures in bounded batches to progressively refresh text overlays.
bool LayoutViewerPanel::ProcessDirtyTexts(double renderZoom, int maxItems, std::vector<unsigned char> &textPixels, bool &timeBudgetExpired) {
  const auto stageStart = std::chrono::steady_clock::now(); int processedCount = 0; const size_t total = currentLayout.textViews.size();
  const uint64_t epoch = renderEpoch_.load();
  for (int pass = 0; pass < 2; ++pass) { for (size_t scanned = 0; scanned < total; ++scanned) { const size_t idx = (rebuildTextCursor_ + scanned) % total; const auto &text = currentLayout.textViews[idx]; wxRect frameRect; const bool isVisible = GetFrameRect(text.frame, frameRect) && frameRect.Intersects(wxRect(wxPoint(0, 0), GetClientSize())); const bool isSelected = selectedElementType == SelectedElementType::Text && selectedElementId == text.id; const bool isPriority = isVisible || isSelected; if ((pass == 0 && !isPriority) || (pass == 1 && isPriority)) continue; TextCache &cache = GetTextCache(text.id); size_t dataHash = HashTextContent(text); if (cache.contentHash != dataHash) cache.renderDirty = true; if (!cache.renderDirty) continue; cache.renderDirty = false; const wxSize renderSize = GetFrameSizeForZoom(text.frame, renderZoom); if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0) { ClearCachedTexture(cache); cache.textureSize = wxSize(0,0); cache.renderZoom = 0.0; } else { auto task = std::async(std::launch::async, [this, text, renderSize, renderZoom]() { return PrepareRgbaPayloadFromImage(BuildTextImage(renderSize, wxSize(text.frame.width, text.frame.height), renderZoom, text), "text"); }); PreparedRgbaPayload payload = task.get(); if (epoch != renderEpoch_.load()) return true; if (!payload.valid) { ClearCachedTexture(cache); cache.textureSize = wxSize(0,0); cache.renderZoom = 0.0; } else { if (!InitGL()) return false; if (cache.texture==0) glGenTextures(1,&cache.texture); glBindTexture(GL_TEXTURE_2D, cache.texture); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); glPixelStorei(GL_UNPACK_ALIGNMENT,1); ScopedActivePixelUnpackPbo scopedPbo(cache.pixelUnpackPbo, cache.pboBytes); if (!UploadRgbaToTexture(cache.texture,payload.width,payload.height,payload.pixels.data(),cache.textureSize,true)) { ClearCachedTexture(cache); cache.textureSize = wxSize(0,0); cache.renderZoom = 0.0; } else { cache.textureSize = wxSize(payload.width,payload.height); cache.renderZoom = renderZoom; cache.contentHash = dataHash; } } } processedCount++; rebuildTextCursor_=(idx+1)%total; if (processedCount>=maxItems || IsRebuildTimeBudgetExpired()) { timeBudgetExpired=true; break; } } if (timeBudgetExpired) break; }
  LogRebuildStageMetrics("texts", processedCount, std::chrono::steady_clock::now()-stageStart); return true;
}

// Processes dirty image textures in bounded batches to progressively refresh bitmap overlays.
bool LayoutViewerPanel::ProcessDirtyImages(double renderZoom, int maxItems, std::vector<unsigned char> &imagePixels, bool &timeBudgetExpired) {
  const auto stageStart = std::chrono::steady_clock::now(); int processedCount = 0; const size_t total = currentLayout.imageViews.size();
  for (int pass = 0; pass < 2; ++pass) { for (size_t scanned = 0; scanned < total; ++scanned) { const size_t idx = (rebuildImageCursor_ + scanned) % total; const auto &image = currentLayout.imageViews[idx]; wxRect frameRect; const bool isVisible = GetFrameRect(image.frame, frameRect) && frameRect.Intersects(wxRect(wxPoint(0, 0), GetClientSize())); const bool isSelected = selectedElementType == SelectedElementType::Image && selectedElementId == image.id; const bool isPriority = isVisible || isSelected; if ((pass == 0 && !isPriority) || (pass == 1 && isPriority)) continue; ImageCache &cache = GetImageCache(image.id); size_t dataHash = HashImageContent(image); if (cache.contentHash != dataHash) cache.renderDirty = true; if (!cache.renderDirty) continue; cache.renderDirty = false; const wxSize renderSize = GetFrameSizeForZoom(image.frame, renderZoom); if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0 || image.imagePath.empty()) { ClearCachedTexture(cache); cache.textureSize = wxSize(0,0); cache.renderZoom = 0.0; } else { std::filesystem::file_time_type fileMtime{}; const bool hasMtime = TryGetImageFileMtime(image.imagePath, fileMtime); if (!hasMtime) { ClearCachedTexture(cache); cache.textureSize = wxSize(0,0); cache.renderZoom = 0.0; } else { const wxImage *scaled = GetOrCreateScaledImageBitmap(image.imagePath, renderSize.GetWidth(), renderSize.GetHeight(), fileMtime); if (!scaled || !scaled->IsOk()) { ClearCachedTexture(cache); cache.textureSize = wxSize(0,0); cache.renderZoom = 0.0; } else { const int width=scaled->GetWidth(), height=scaled->GetHeight(); const unsigned char *rgb=scaled->GetData(), *alpha=scaled->GetAlpha(); if (!rgb || width<=0 || height<=0 || !TryAllocatePixelBuffer(imagePixels,width,height,"image")) { ClearCachedTexture(cache); cache.textureSize = wxSize(0,0); cache.renderZoom = 0.0; } else { for (int i=0;i<width*height;++i){ imagePixels[(size_t)i*4]=rgb[i*3]; imagePixels[(size_t)i*4+1]=rgb[i*3+1]; imagePixels[(size_t)i*4+2]=rgb[i*3+2]; imagePixels[(size_t)i*4+3]=alpha?alpha[i]:255; } if (!InitGL()) return false; if (cache.texture==0) glGenTextures(1,&cache.texture); glBindTexture(GL_TEXTURE_2D, cache.texture); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); glPixelStorei(GL_UNPACK_ALIGNMENT, 1); ScopedActivePixelUnpackPbo scopedPbo(cache.pixelUnpackPbo, cache.pboBytes); if (!UploadRgbaToTexture(cache.texture,width,height,imagePixels.data(),cache.textureSize,true)) { ClearCachedTexture(cache); cache.textureSize = wxSize(0,0); cache.renderZoom = 0.0; } else { cache.textureSize = wxSize(width,height); cache.renderZoom = renderZoom; cache.contentHash = dataHash; } imagePixels.clear(); } } } }
    processedCount++; rebuildImageCursor_=(idx+1)%total; if (processedCount>=maxItems || IsRebuildTimeBudgetExpired()) { timeBudgetExpired=true; break; } } if (timeBudgetExpired) break;
  }
  LogRebuildStageMetrics("images", processedCount, std::chrono::steady_clock::now()-stageStart); return true;
}

void LayoutViewerPanel::ClearCachedTexture() {
  for (auto &entry : viewCaches_) {
    ClearCachedTexture(entry.second);
  }
  viewCaches_.clear();
  for (auto &entry : legendCaches_) {
    ClearCachedTexture(entry.second);
  }
  legendCaches_.clear();
  for (auto &entry : eventTableCaches_) {
    ClearCachedTexture(entry.second);
  }
  eventTableCaches_.clear();
  for (auto &entry : textCaches_) {
    ClearCachedTexture(entry.second);
  }
  textCaches_.clear();
  for (auto &entry : imageCaches_) {
    ClearCachedTexture(entry.second);
  }
  imageCaches_.clear();
}

void LayoutViewerPanel::ClearCachedTexture(ViewCache &cache) {
  if (cache.texture == 0 && cache.pixelUnpackPbo == 0)
    return;
  if (!glContext_) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  if (!IsShown()) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  SetCurrent(*glContext_);
  if (cache.texture != 0) {
    glDeleteTextures(1, &cache.texture);
    cache.texture = 0;
  }
  if (cache.pixelUnpackPbo != 0) {
    glDeleteBuffers(1, &cache.pixelUnpackPbo);
    cache.pixelUnpackPbo = 0;
  }
  cache.pboBytes = 0;
}

void LayoutViewerPanel::ClearCachedTexture(LegendCache &cache) {
  if (cache.texture == 0 && cache.pixelUnpackPbo == 0)
    return;
  if (!glContext_) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  if (!IsShown()) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  SetCurrent(*glContext_);
  if (cache.texture != 0) {
    glDeleteTextures(1, &cache.texture);
    cache.texture = 0;
  }
  if (cache.pixelUnpackPbo != 0) {
    glDeleteBuffers(1, &cache.pixelUnpackPbo);
    cache.pixelUnpackPbo = 0;
  }
  cache.pboBytes = 0;
}

void LayoutViewerPanel::ClearCachedTexture(EventTableCache &cache) {
  if (cache.texture == 0 && cache.pixelUnpackPbo == 0)
    return;
  if (!glContext_) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  if (!IsShown()) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  SetCurrent(*glContext_);
  if (cache.texture != 0) {
    glDeleteTextures(1, &cache.texture);
    cache.texture = 0;
  }
  if (cache.pixelUnpackPbo != 0) {
    glDeleteBuffers(1, &cache.pixelUnpackPbo);
    cache.pixelUnpackPbo = 0;
  }
  cache.pboBytes = 0;
}

void LayoutViewerPanel::ClearCachedTexture(TextCache &cache) {
  if (cache.texture == 0 && cache.pixelUnpackPbo == 0)
    return;
  if (!glContext_) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  if (!IsShown()) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  SetCurrent(*glContext_);
  if (cache.texture != 0) {
    glDeleteTextures(1, &cache.texture);
    cache.texture = 0;
  }
  if (cache.pixelUnpackPbo != 0) {
    glDeleteBuffers(1, &cache.pixelUnpackPbo);
    cache.pixelUnpackPbo = 0;
  }
  cache.pboBytes = 0;
}

void LayoutViewerPanel::ClearCachedTexture(ImageCache &cache) {
  if (cache.texture == 0 && cache.pixelUnpackPbo == 0)
    return;
  if (!glContext_) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  if (!IsShown()) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  SetCurrent(*glContext_);
  if (cache.texture != 0) {
    glDeleteTextures(1, &cache.texture);
    cache.texture = 0;
  }
  if (cache.pixelUnpackPbo != 0) {
    glDeleteBuffers(1, &cache.pixelUnpackPbo);
    cache.pixelUnpackPbo = 0;
  }
  cache.pboBytes = 0;
}

bool LayoutViewerPanel::HasDirtyRenderCaches() const {
  auto hasDirty = [](const auto &map) {
    for (const auto &entry : map) {
      if (entry.second.renderDirty)
        return true;
    }
    return false;
  };
  return hasDirty(viewCaches_) || hasDirty(legendCaches_) ||
         hasDirty(eventTableCaches_) || hasDirty(textCaches_) ||
         hasDirty(imageCaches_);
}

bool LayoutViewerPanel::NeedsRenderRebuild() const {
  return renderDirty || contentDirty || HasDirtyRenderCaches();
}

// Schedules a deferred layout texture rebuild when content is dirty and rendering is available.
void LayoutViewerPanel::RequestRenderRebuild() {
  // Advances the render epoch to cancel stale payloads queued before this rebuild request.
  NextRenderEpoch();
  if (IsLayoutEmpty()) {
    renderPending = false;
    loadingRequested = false;
    isLoading = false;
    return;
  }
  if (!isReadyToRender_ || !glContext_ || !IsShownOnScreen())
    return;
  auto *mw = MainWindow::Instance();
  if (!mw)
    return;
  auto *offscreenRenderer = mw->GetOffscreenRenderer();
  if (!offscreenRenderer || !offscreenRenderer->GetPanel())
    return;
  if (!NeedsRenderRebuild() || renderPending)
    return;
  renderPending = true;
  loadingRequested = true;
  if (!loadingTimer_.IsRunning()) {
    loadingTimer_.StartOnce(kLoadingOverlayDelayMs);
  }
  wxWeakRef<LayoutViewerPanel> weakThis(this);
  CallAfter([weakThis]() {
    if (!weakThis)
      return;
    LayoutViewerPanel *panel = weakThis.get();
    if (!panel)
      return;
    panel->isLoading = true;
    panel->Refresh();
    panel->Update();
    if (!weakThis)
      return;
    panel = weakThis.get();
    if (!panel)
      return;
    if (panel->renderDelayTimer_.IsRunning()) {
      panel->renderDelayTimer_.Stop();
    }
    panel->renderDelayTimer_.StartOnce(kLoadingOverlayDelayMs);
  });
}

// Returns a new render epoch value used to guard asynchronous CPU payload tasks.
uint64_t LayoutViewerPanel::NextRenderEpoch() { return ++renderEpoch_; }

void LayoutViewerPanel::OnLoadingTimer(wxTimerEvent &) {
  if (!loadingRequested)
    return;
  if (!renderPending && !NeedsRenderRebuild())
    return;
  isLoading = true;
  Refresh();
}

void LayoutViewerPanel::OnRenderDelayTimer(wxTimerEvent &) {
  if (!renderPending)
    return;
  if (!NeedsRenderRebuild()) {
    renderPending = false;
    return;
  }
  wxWeakRef<LayoutViewerPanel> weakThis(this);
  CallAfter([weakThis]() {
    if (!weakThis)
      return;
    LayoutViewerPanel *panel = weakThis.get();
    if (!panel)
      return;
    panel->renderPending = false;
    panel->RebuildCachedTexture();
    panel->Refresh();
  });
}

bool LayoutViewerPanel::AreTexturesReady() const {
  auto hasTexture = [](const auto &map, int id) {
    auto it = map.find(id);
    return it != map.end() && it->second.texture != 0;
  };

  for (const auto &view : currentLayout.view2dViews) {
    if (!hasTexture(viewCaches_, view.id))
      return false;
  }
  for (const auto &legend : currentLayout.legendViews) {
    if (!hasTexture(legendCaches_, legend.id))
      return false;
  }
  for (const auto &table : currentLayout.eventTables) {
    if (!hasTexture(eventTableCaches_, table.id))
      return false;
  }
  for (const auto &text : currentLayout.textViews) {
    if (!hasTexture(textCaches_, text.id))
      return false;
  }
  for (const auto &image : currentLayout.imageViews) {
    if (!hasTexture(imageCaches_, image.id))
      return false;
  }
  return true;
}

bool LayoutViewerPanel::SelectElementAtPosition(const wxPoint &pos) {
  const auto applySelectionChange = [&](SelectedElementType newType, int newId,
                                        bool emitViewSelectionChanged) {
    const bool selectionOnlyChange =
        selectedElementType != newType || selectedElementId != newId;
    const bool renderableContentChanged = false;

    selectedElementType = newType;
    selectedElementId = newId;

    if (emitViewSelectionChanged) {
      EmitViewSelectionChanged(newId);
    }

    if (selectionOnlyChange && !renderableContentChanged) {
      RefreshAfterSelectionOnlyUpdate();
      return;
    }

    RequestRenderRebuild();
    Refresh();
  };

  EnsureSelectionIndexCache();
  const auto &elements = selectionIndexCache_.zOrderedElements;
  const auto &viewById = selectionIndexCache_.viewById;
  const auto &legendById = selectionIndexCache_.legendById;
  const auto &eventTableById = selectionIndexCache_.eventTableById;
  const auto &textById = selectionIndexCache_.textById;
  const auto &imageById = selectionIndexCache_.imageById;
  for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
    if (it->type == SelectedElementType::Legend) {
      auto legendIt = legendById.find(it->id);
      if (legendIt == legendById.end())
        continue;
      const auto *legend = legendIt->second;
      wxRect frameRect;
      if (!GetFrameRect(legend->frame, frameRect))
        continue;
      if (!frameRect.Contains(pos))
        continue;
      if (selectedElementType == SelectedElementType::Legend &&
          selectedElementId == legend->id) {
        return true;
      }
      applySelectionChange(SelectedElementType::Legend, legend->id,
                           /*emitViewSelectionChanged=*/false);
      return true;
    }

    if (it->type == SelectedElementType::EventTable) {
      auto tableIt = eventTableById.find(it->id);
      if (tableIt == eventTableById.end())
        continue;
      const auto *table = tableIt->second;
      wxRect frameRect;
      if (!GetFrameRect(table->frame, frameRect))
        continue;
      if (!frameRect.Contains(pos))
        continue;
      if (selectedElementType == SelectedElementType::EventTable &&
          selectedElementId == table->id) {
        return true;
      }
      applySelectionChange(SelectedElementType::EventTable, table->id,
                           /*emitViewSelectionChanged=*/false);
      return true;
    }

    if (it->type == SelectedElementType::Text) {
      auto textIt = textById.find(it->id);
      if (textIt == textById.end())
        continue;
      const auto *text = textIt->second;
      wxRect frameRect;
      if (!GetFrameRect(text->frame, frameRect))
        continue;
      if (!frameRect.Contains(pos))
        continue;
      if (selectedElementType == SelectedElementType::Text &&
          selectedElementId == text->id) {
        return true;
      }
      applySelectionChange(SelectedElementType::Text, text->id,
                           /*emitViewSelectionChanged=*/false);
      return true;
    }

    if (it->type == SelectedElementType::Image) {
      auto imageIt = imageById.find(it->id);
      if (imageIt == imageById.end())
        continue;
      const auto *image = imageIt->second;
      wxRect frameRect;
      if (!GetFrameRect(image->frame, frameRect))
        continue;
      if (!frameRect.Contains(pos))
        continue;
      if (selectedElementType == SelectedElementType::Image &&
          selectedElementId == image->id) {
        return true;
      }
      applySelectionChange(SelectedElementType::Image, image->id,
                           /*emitViewSelectionChanged=*/false);
      return true;
    }

    if (it->type == SelectedElementType::View2D) {
      auto viewIt = viewById.find(it->id);
      if (viewIt == viewById.end())
        continue;
      const auto *view = viewIt->second;
      wxRect frameRect;
      if (!GetFrameRect(view->frame, frameRect))
        continue;
      if (!frameRect.Contains(pos))
        continue;
      if (selectedElementType == SelectedElementType::View2D &&
          selectedElementId == view->id) {
        return true;
      }
      applySelectionChange(SelectedElementType::View2D, view->id,
                           /*emitViewSelectionChanged=*/true);
      return true;
    }
  }
  return false;
}

LayoutViewerPanel::FrameDragMode
LayoutViewerPanel::HitTestFrame(const wxPoint &pos,
                                const wxRect &frameRect) const {
  wxRect handleRight(frameRect.GetRight() - kHandleHalfPx - kHandleHoverPadPx,
                     frameRect.GetTop() + frameRect.GetHeight() / 2 -
                         kHandleHalfPx - kHandleHoverPadPx,
                     kHandleSizePx + kHandleHoverPadPx * 2,
                     kHandleSizePx + kHandleHoverPadPx * 2);
  wxRect handleBottom(frameRect.GetLeft() + frameRect.GetWidth() / 2 -
                          kHandleHalfPx - kHandleHoverPadPx,
                      frameRect.GetBottom() - kHandleHalfPx - kHandleHoverPadPx,
                      kHandleSizePx + kHandleHoverPadPx * 2,
                      kHandleSizePx + kHandleHoverPadPx * 2);
  wxRect handleCorner(frameRect.GetRight() - kHandleHalfPx - kHandleHoverPadPx,
                      frameRect.GetBottom() - kHandleHalfPx -
                          kHandleHoverPadPx,
                      kHandleSizePx + kHandleHoverPadPx * 2,
                      kHandleSizePx + kHandleHoverPadPx * 2);

  if (handleCorner.Contains(pos))
    return FrameDragMode::ResizeCorner;
  if (handleRight.Contains(pos))
    return FrameDragMode::ResizeRight;
  if (handleBottom.Contains(pos))
    return FrameDragMode::ResizeBottom;
  if (frameRect.Contains(pos))
    return FrameDragMode::Move;
  return FrameDragMode::None;
}

wxCursor LayoutViewerPanel::CursorForMode(FrameDragMode mode) const {
  switch (mode) {
  case FrameDragMode::ResizeRight:
    return wxCursor(wxCURSOR_SIZEWE);
  case FrameDragMode::ResizeBottom:
    return wxCursor(wxCURSOR_SIZENS);
  case FrameDragMode::ResizeCorner:
    return wxCursor(wxCURSOR_SIZENWSE);
  case FrameDragMode::Move:
    return wxCursor(wxCURSOR_SIZING);
  case FrameDragMode::None:
  default:
    return wxCursor(wxCURSOR_ARROW);
  }
}

void LayoutViewerPanel::EmitEditViewRequest() {
  wxCommandEvent event(EVT_LAYOUT_VIEW_EDIT);
  event.SetEventObject(this);
  ProcessWindowEvent(event);
}

void LayoutViewerPanel::EmitViewSelectionChanged(int viewId) {
  if (viewId <= 0)
    return;
  wxCommandEvent event(EVT_LAYOUT_VIEW_SELECTED);
  event.SetEventObject(this);
  event.SetInt(viewId);
  ProcessWindowEvent(event);
}

LayoutViewerPanel::ViewCache &LayoutViewerPanel::GetViewCache(int viewId) {
  auto [it, inserted] = viewCaches_.try_emplace(viewId, ViewCache{});
  if (inserted) {
    contentDirty = true;
  }
  return it->second;
}

LayoutViewerPanel::LegendCache &LayoutViewerPanel::GetLegendCache(int legendId) {
  auto [it, inserted] = legendCaches_.try_emplace(legendId, LegendCache{});
  if (inserted) {
    contentDirty = true;
  }
  return it->second;
}

LayoutViewerPanel::EventTableCache &
LayoutViewerPanel::GetEventTableCache(int tableId) {
  auto [it, inserted] =
      eventTableCaches_.try_emplace(tableId, EventTableCache{});
  if (inserted) {
    contentDirty = true;
  }
  return it->second;
}

LayoutViewerPanel::TextCache &LayoutViewerPanel::GetTextCache(int textId) {
  auto [it, inserted] = textCaches_.try_emplace(textId, TextCache{});
  if (inserted) {
    contentDirty = true;
  }
  return it->second;
}

LayoutViewerPanel::ImageCache &LayoutViewerPanel::GetImageCache(int imageId) {
  auto [it, inserted] = imageCaches_.try_emplace(imageId, ImageCache{});
  if (inserted) {
    contentDirty = true;
  }
  return it->second;
}
