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
#include "viewer2doffscreenrenderer.h"

#include "configmanager.h"
#include "viewer2dstate.h"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace {
constexpr int kDefaultViewportWidth = 1600;
constexpr int kDefaultViewportHeight = 900;

void HashCombine(size_t &seed, size_t value) {
  seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename T> void HashCombineValue(size_t &seed, const T &value) {
  HashCombine(seed, std::hash<T>{}(value));
}

void HashMatrix(size_t &seed, const Matrix &matrix) {
  for (float value : matrix.u)
    HashCombineValue(seed, value);
  for (float value : matrix.v)
    HashCombineValue(seed, value);
  for (float value : matrix.w)
    HashCombineValue(seed, value);
  for (float value : matrix.o)
    HashCombineValue(seed, value);
}

size_t HashViewer2DState(const viewer2d::Viewer2DState &state) {
  size_t seed = 0;
  HashCombineValue(seed, state.camera.offsetPixelsX);
  HashCombineValue(seed, state.camera.offsetPixelsY);
  HashCombineValue(seed, state.camera.zoom);
  HashCombineValue(seed, state.camera.viewportWidth);
  HashCombineValue(seed, state.camera.viewportHeight);
  HashCombineValue(seed, state.camera.view);

  HashCombineValue(seed, state.renderOptions.renderMode);
  HashCombineValue(seed, state.renderOptions.darkMode);
  HashCombineValue(seed, state.renderOptions.showGrid);
  HashCombineValue(seed, state.renderOptions.gridStyle);
  HashCombineValue(seed, state.renderOptions.gridColorR);
  HashCombineValue(seed, state.renderOptions.gridColorG);
  HashCombineValue(seed, state.renderOptions.gridColorB);
  HashCombineValue(seed, state.renderOptions.gridDrawAbove);

  for (bool value : state.renderOptions.showLabelName)
    HashCombineValue(seed, value);
  for (bool value : state.renderOptions.showLabelId)
    HashCombineValue(seed, value);
  for (bool value : state.renderOptions.showLabelDmx)
    HashCombineValue(seed, value);

  HashCombineValue(seed, state.renderOptions.labelFontSizeName);
  HashCombineValue(seed, state.renderOptions.labelFontSizeId);
  HashCombineValue(seed, state.renderOptions.labelFontSizeDmx);
  for (float value : state.renderOptions.labelOffsetDistance)
    HashCombineValue(seed, value);
  for (float value : state.renderOptions.labelOffsetAngle)
    HashCombineValue(seed, value);

  HashCombineValue(seed, state.layers.hiddenLayers.size());
  for (const auto &layer : state.layers.hiddenLayers)
    HashCombineValue(seed, layer);

  return seed;
}

template <typename MapType>
std::vector<std::pair<std::string, typename MapType::mapped_type>>
SortedEntries(const MapType &map) {
  std::vector<std::pair<std::string, typename MapType::mapped_type>> entries;
  entries.reserve(map.size());
  for (const auto &entry : map)
    entries.push_back(entry);
  std::sort(entries.begin(), entries.end(),
            [](const auto &lhs, const auto &rhs) { return lhs.first < rhs.first; });
  return entries;
}

size_t HashScene(const MvrScene &scene) {
  size_t seed = 0;
  HashCombineValue(seed, scene.basePath);
  HashCombineValue(seed, scene.provider);
  HashCombineValue(seed, scene.providerVersion);
  HashCombineValue(seed, scene.versionMajor);
  HashCombineValue(seed, scene.versionMinor);

  HashCombineValue(seed, scene.fixtures.size());
  for (const auto &entry : SortedEntries(scene.fixtures)) {
    const Fixture &fixture = entry.second;
    HashCombineValue(seed, entry.first);
    HashCombineValue(seed, fixture.uuid);
    HashCombineValue(seed, fixture.instanceName);
    HashCombineValue(seed, fixture.typeName);
    HashCombineValue(seed, fixture.gdtfSpec);
    HashCombineValue(seed, fixture.gdtfMode);
    HashCombineValue(seed, fixture.focus);
    HashCombineValue(seed, fixture.function);
    HashCombineValue(seed, fixture.layer);
    HashCombineValue(seed, fixture.position);
    HashCombineValue(seed, fixture.positionName);
    HashCombineValue(seed, fixture.address);
    HashCombineValue(seed, fixture.matrixRaw);
    HashCombineValue(seed, fixture.color);
    HashCombineValue(seed, fixture.fixtureId);
    HashCombineValue(seed, fixture.fixtureIdNumeric);
    HashCombineValue(seed, fixture.unitNumber);
    HashCombineValue(seed, fixture.customId);
    HashCombineValue(seed, fixture.customIdType);
    HashCombineValue(seed, fixture.dmxInvertPan);
    HashCombineValue(seed, fixture.dmxInvertTilt);
    HashCombineValue(seed, fixture.powerConsumptionW);
    HashCombineValue(seed, fixture.weightKg);
    HashMatrix(seed, fixture.transform);
  }

  HashCombineValue(seed, scene.trusses.size());
  for (const auto &entry : SortedEntries(scene.trusses)) {
    const Truss &truss = entry.second;
    HashCombineValue(seed, entry.first);
    HashCombineValue(seed, truss.uuid);
    HashCombineValue(seed, truss.name);
    HashCombineValue(seed, truss.gdtfSpec);
    HashCombineValue(seed, truss.gdtfMode);
    HashCombineValue(seed, truss.symbolFile);
    HashCombineValue(seed, truss.modelFile);
    HashCombineValue(seed, truss.function);
    HashCombineValue(seed, truss.layer);
    HashCombineValue(seed, truss.manufacturer);
    HashCombineValue(seed, truss.model);
    HashCombineValue(seed, truss.lengthMm);
    HashCombineValue(seed, truss.widthMm);
    HashCombineValue(seed, truss.heightMm);
    HashCombineValue(seed, truss.weightKg);
    HashCombineValue(seed, truss.crossSection);
    HashCombineValue(seed, truss.position);
    HashCombineValue(seed, truss.positionName);
    HashCombineValue(seed, truss.unitNumber);
    HashCombineValue(seed, truss.customId);
    HashCombineValue(seed, truss.customIdType);
    HashMatrix(seed, truss.transform);
  }

  HashCombineValue(seed, scene.supports.size());
  for (const auto &entry : SortedEntries(scene.supports)) {
    const Support &support = entry.second;
    HashCombineValue(seed, entry.first);
    HashCombineValue(seed, support.uuid);
    HashCombineValue(seed, support.name);
    HashCombineValue(seed, support.gdtfSpec);
    HashCombineValue(seed, support.gdtfMode);
    HashCombineValue(seed, support.function);
    HashCombineValue(seed, support.chainLength);
    HashCombineValue(seed, support.position);
    HashCombineValue(seed, support.positionName);
    HashCombineValue(seed, support.layer);
    HashCombineValue(seed, support.capacityKg);
    HashCombineValue(seed, support.weightKg);
    HashCombineValue(seed, support.hoistFunction);
    HashMatrix(seed, support.transform);
  }

  HashCombineValue(seed, scene.sceneObjects.size());
  for (const auto &entry : SortedEntries(scene.sceneObjects)) {
    const SceneObject &object = entry.second;
    HashCombineValue(seed, entry.first);
    HashCombineValue(seed, object.uuid);
    HashCombineValue(seed, object.name);
    HashCombineValue(seed, object.layer);
    HashCombineValue(seed, object.modelFile);
    HashMatrix(seed, object.transform);
  }

  HashCombineValue(seed, scene.layers.size());
  for (const auto &entry : SortedEntries(scene.layers)) {
    const Layer &layer = entry.second;
    HashCombineValue(seed, entry.first);
    HashCombineValue(seed, layer.uuid);
    HashCombineValue(seed, layer.name);
    HashCombineValue(seed, layer.color);
    HashCombineValue(seed, layer.childUUIDs.size());
    for (const auto &child : layer.childUUIDs)
      HashCombineValue(seed, child);
  }

  for (const auto &entry : SortedEntries(scene.positions)) {
    HashCombineValue(seed, entry.first);
    HashCombineValue(seed, entry.second);
  }
  for (const auto &entry : SortedEntries(scene.symdefFiles)) {
    HashCombineValue(seed, entry.first);
    HashCombineValue(seed, entry.second);
  }
  for (const auto &entry : SortedEntries(scene.symdefTypes)) {
    HashCombineValue(seed, entry.first);
    HashCombineValue(seed, entry.second);
  }

  return seed;
}

size_t HashViewer2DStateWithScene() {
  ConfigManager &cfg = ConfigManager::Get();
  viewer2d::Viewer2DState state = viewer2d::CaptureState(nullptr, cfg);
  size_t seed = HashViewer2DState(state);
  HashCombine(seed, HashScene(cfg.GetScene()));
  return seed;
}
}

Viewer2DOffscreenRenderer::Viewer2DOffscreenRenderer(wxWindow *parent) {
  parent_ = parent;
  CreatePanel();
}

Viewer2DOffscreenRenderer::~Viewer2DOffscreenRenderer() { DestroyPanel(); }

void Viewer2DOffscreenRenderer::SetSharedContext(wxGLContext *sharedContext) {
  if (sharedContext_ == sharedContext)
    return;
  sharedContext_ = sharedContext;
  DestroyPanel();
  CreatePanel();
}

void Viewer2DOffscreenRenderer::SetViewportSize(const wxSize &size) {
  if (!panel_)
    return;
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return;
  panel_->SetSize(size);
  panel_->SetClientSize(size);
}

void Viewer2DOffscreenRenderer::PrepareForCapture(const wxSize &size) {
  if (!panel_)
    return;
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return;
  SetViewportSize(size);
  panel_->EnsureGLReady();
  EnsureRenderTarget(size);
  size_t currentHash = HashViewer2DStateWithScene();
  if (currentHash != lastStateHash_) {
    panel_->LoadViewFromConfig();
    panel_->UpdateScene(true);
    lastStateHash_ = currentHash;
  }
  panel_->RenderToTexture(fbo_, size);
}

bool Viewer2DOffscreenRenderer::RenderToTexture(const wxSize &size) {
  if (!panel_)
    return false;
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return false;

  PrepareForCapture(size);
  return fbo_ != 0 && colorTex_ != 0;
}

void Viewer2DOffscreenRenderer::CreatePanel() {
  if (!parent_)
    return;
  host_ = new wxPanel(parent_, wxID_ANY, wxDefaultPosition, wxSize(1, 1));
  host_->Hide();

  panel_ = new Viewer2DPanel(host_, true, false, false, sharedContext_);
  panel_->SetSize(wxSize(kDefaultViewportWidth, kDefaultViewportHeight));
  panel_->SetClientSize(wxSize(kDefaultViewportWidth, kDefaultViewportHeight));
  panel_->LoadViewFromConfig();
  panel_->UpdateScene(true);
  lastStateHash_ = HashViewer2DStateWithScene();
}

void Viewer2DOffscreenRenderer::DestroyPanel() {
  DestroyRenderTarget();
  if (panel_) {
    panel_->Destroy();
    panel_ = nullptr;
  }
  if (host_) {
    host_->Destroy();
    host_ = nullptr;
  }
  lastStateHash_ = 0;
}

void Viewer2DOffscreenRenderer::EnsureRenderTarget(const wxSize &size) {
  if (!panel_)
    return;

  if (colorTex_ == 0) {
    glGenTextures(1, &colorTex_);
  }
  if (fbo_ == 0) {
    glGenFramebuffers(1, &fbo_);
  }
  if (depthRb_ == 0) {
    glGenRenderbuffers(1, &depthRb_);
  }

  if (renderSize_ != size) {
    renderSize_ = size;
    glBindTexture(GL_TEXTURE_2D, colorTex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.GetWidth(),
                 size.GetHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindRenderbuffer(GL_RENDERBUFFER, depthRb_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                          size.GetWidth(), size.GetHeight());
  }

  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         colorTex_, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, depthRb_);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Viewer2DOffscreenRenderer::DestroyRenderTarget() {
  if (panel_) {
    panel_->EnsureGLReady();
  }
  if (depthRb_ != 0) {
    glDeleteRenderbuffers(1, &depthRb_);
    depthRb_ = 0;
  }
  if (fbo_ != 0) {
    glDeleteFramebuffers(1, &fbo_);
    fbo_ = 0;
  }
  if (colorTex_ != 0) {
    glDeleteTextures(1, &colorTex_);
    colorTex_ = 0;
  }
  renderSize_ = wxSize(0, 0);
}

Viewer2DOffscreenRendererPool::Viewer2DOffscreenRendererPool(wxWindow *parent)
    : parent_(parent) {}

Viewer2DOffscreenRenderer *Viewer2DOffscreenRendererPool::GetRendererForView(
    int viewId) {
  auto it = rendererIndexByView_.find(viewId);
  if (it != rendererIndexByView_.end()) {
    return renderers_[it->second].get();
  }

  auto renderer = std::make_unique<Viewer2DOffscreenRenderer>(parent_);
  if (sharedContext_) {
    renderer->SetSharedContext(sharedContext_);
  }
  renderers_.push_back(std::move(renderer));
  const size_t index = renderers_.size() - 1;
  rendererIndexByView_[viewId] = index;
  return renderers_[index].get();
}

void Viewer2DOffscreenRendererPool::SetSharedContext(
    wxGLContext *sharedContext) {
  sharedContext_ = sharedContext;
  for (auto &renderer : renderers_) {
    if (renderer) {
      renderer->SetSharedContext(sharedContext_);
    }
  }
}
