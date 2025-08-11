/*
 * File: viewer3dcontroller.cpp
 * Author: Luisma Peramato
 * License: MIT
 * Description: Implementation of 3D viewer logic.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "configmanager.h"
#include "loader3ds.h"
#include "loaderglb.h"
#include "scenedatamanager.h"
#include "viewer3dcontroller.h"
// Include shared Matrix type used throughout models
#include "../models/types.h"
#include "consolepanel.h"

#include <wx/wx.h>
#include <wx/tokenzr.h>
#define NANOVG_GL2_IMPLEMENTATION
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <nanovg.h>
#include <nanovg_gl.h>
#include <sstream>

namespace fs = std::filesystem;

// Font size for on-screen labels drawn with NanoVG in the 3D viewer
static constexpr float LABEL_FONT_SIZE_3D = 18.0f;
// Font size for labels in the 2D top-down viewer
static constexpr float LABEL_FONT_SIZE_2D = 2.0f;
// Maximum width for on-screen labels before wrapping
static constexpr float LABEL_MAX_WIDTH = 300.0f;
// Width of fixture labels in meters for the 2D view
// Pixels per meter used by the 2D view
static constexpr float PIXELS_PER_METER = 25.0f;
static std::string FindFileRecursive(const std::string &baseDir,
                                     const std::string &fileName) {
  if (baseDir.empty())
    return {};
  for (auto &p : fs::recursive_directory_iterator(baseDir)) {
    if (!p.is_regular_file())
      continue;
    if (p.path().filename() == fileName)
      return p.path().string();
  }
  return {};
}

// Replace Windows path separators with the platform preferred one
static std::string NormalizePath(const std::string &p) {
  std::string out = p;
  char sep = static_cast<char>(fs::path::preferred_separator);
  std::replace(out.begin(), out.end(), '\\', sep);
  return out;
}

static std::string ResolveGdtfPath(const std::string &base,
                                   const std::string &spec) {
  std::string norm = NormalizePath(spec);
  fs::path p = base.empty() ? fs::path(norm) : fs::path(base) / norm;
  if (fs::exists(p))
    return p.string();
  return FindFileRecursive(base, fs::path(norm).filename().string());
}

static std::string ResolveModelPath(const std::string &base,
                                    const std::string &file) {
  if (file.empty())
    return {};

  std::string norm = NormalizePath(file);
  fs::path p = base.empty() ? fs::path(norm) : fs::path(base) / norm;
  if (fs::exists(p))
    return p.string();

  if (p.extension().empty()) {
    fs::path p3ds = p;
    p3ds += ".3ds";
    if (fs::exists(p3ds))
      return p3ds.string();

    fs::path pglb = p;
    pglb += ".glb";
    if (fs::exists(pglb))
      return pglb.string();

    std::string fn3ds = FindFileRecursive(base, p.filename().string() + ".3ds");
    if (!fn3ds.empty())
      return fn3ds;
    return FindFileRecursive(base, p.filename().string() + ".glb");
  }

  return FindFileRecursive(base, p.filename().string());
}

static std::string FormatMeters(float mm) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2) << mm / 1000.0f;
  std::string s = oss.str();
  s.erase(s.find_last_not_of('0') + 1, std::string::npos);
  if (!s.empty() && s.back() == '.')
    s.pop_back();
  return s;
}

static void MatrixToArray(const Matrix &m, float out[16]) {
  out[0] = m.u[0];
  out[1] = m.u[1];
  out[2] = m.u[2];
  out[3] = 0.0f;
  out[4] = m.v[0];
  out[5] = m.v[1];
  out[6] = m.v[2];
  out[7] = 0.0f;
  out[8] = m.w[0];
  out[9] = m.w[1];
  out[10] = m.w[2];
  out[11] = 0.0f;
  out[12] = m.o[0];
  out[13] = m.o[1];
  out[14] = m.o[2];
  out[15] = 1.0f;
}

struct ScreenRect {
  double minX = DBL_MAX;
  double minY = DBL_MAX;
  double maxX = -DBL_MAX;
  double maxY = -DBL_MAX;
};

// Inserts a line break every two words in the provided text.
static wxString WrapEveryTwoWords(const wxString &text) {
  wxStringTokenizer tk(text, " ");
  wxString result;
  int count = 0;
  while (tk.HasMoreTokens()) {
    if (count > 0) {
      if (count % 2 == 0)
        result += "\n";
      else
        result += " ";
    }
    result += tk.GetNextToken();
    ++count;
  }
  return result;
}

// Draws a text string at screen coordinates using NanoVG. The font size and
// maximum width are specified in pixels.
static void DrawText2D(NVGcontext *vg, int font, const std::string &text, int x,
                       int y, float fontSize = LABEL_FONT_SIZE_3D,
                       float maxWidth = LABEL_MAX_WIDTH,
                       bool drawBorder = true) {
  if (!vg || font < 0 || text.empty())
    return;

  GLint vp[4];
  glGetIntegerv(GL_VIEWPORT, vp);

  nvgBeginFrame(vg, vp[2], vp[3], 1.0f);
  nvgSave(vg);
  nvgFontSize(vg, fontSize);
  nvgFontFaceId(vg, font);
  // Center text for multiline labels
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

  // Determine the width based on the actual text content so the background
  // box tightly fits the rendered label. We measure each line separately to
  // avoid the fixed width imposed by nvgTextBoxBounds and clamp to a maximum
  // so long names wrap instead of growing indefinitely.

  float textWidth = 0.0f;
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find('\n', start);
    std::string line = text.substr(start, end - start);
    float lb[4];
    nvgTextBounds(vg, 0.f, 0.f, line.c_str(), nullptr, lb);
    textWidth = std::max(textWidth, lb[2] - lb[0]);
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  if (maxWidth > 0.0f)
    textWidth = std::min(textWidth, maxWidth);
  const int padding = 4;

  // Calculate the exact bounding box for the text using the same alignment
  // and width that will be used when rendering it. This ensures the
  // background rectangle matches the visual position of the text.
  float bounds[4];
  nvgTextBoxBounds(vg, (float)x, (float)y, textWidth, text.c_str(), nullptr,
                   bounds);

  nvgBeginPath(vg);
  nvgRect(vg, bounds[0] - padding, bounds[1] - padding,
          (bounds[2] - bounds[0]) + padding * 2,
          (bounds[3] - bounds[1]) + padding * 2);
  nvgFillColor(vg, nvgRGBAf(0.f, 0.f, 0.f, 0.6f));
  nvgFill(vg);

  if (drawBorder) {
    nvgBeginPath(vg);
    nvgRect(vg, bounds[0] - padding, bounds[1] - padding,
            (bounds[2] - bounds[0]) + padding * 2,
            (bounds[3] - bounds[1]) + padding * 2);
    nvgStrokeColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.8f));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);
  }

  nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 1.f));
  // Draw multi-line label using textWidth to avoid excessive empty space.
  nvgTextBox(vg, (float)x, (float)y, textWidth, text.c_str(), nullptr);
  nvgRestore(vg);
  nvgEndFrame(vg);
}

static std::array<float, 3> TransformPoint(const Matrix &m,
                                           const std::array<float, 3> &p) {
  return {m.u[0] * p[0] + m.v[0] * p[1] + m.w[0] * p[2] + m.o[0],
          m.u[1] * p[0] + m.v[1] * p[1] + m.w[1] * p[2] + m.o[1],
          m.u[2] * p[0] + m.v[2] * p[1] + m.w[2] * p[2] + m.o[2]};
}

Viewer3DController::Viewer3DController() {
  // Actual initialization of OpenGL-dependent resources is delayed
  // until a valid context is available.
}

Viewer3DController::~Viewer3DController() {
  if (m_vg)
    nvgDeleteGL2(m_vg);
}

void Viewer3DController::InitializeGL() {
  if (m_vg)
    return; // Already initialized

  m_vg = nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
  if (m_vg) {
    const char *fontPaths[] = {
#ifdef _WIN32
        "C:/Windows/Fonts/arial.ttf",
#endif
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", nullptr};

    for (const char **p = fontPaths; *p; ++p) {
      if (fs::exists(*p)) {
        m_font = nvgCreateFont(m_vg, "sans", *p);
        if (m_font >= 0)
          break;
      }
    }
    if (m_font < 0)
      std::cerr << "Failed to load font for labels" << std::endl;
  } else {
    std::cerr << "Failed to create NanoVG context" << std::endl;
  }
}

void Viewer3DController::SetHighlightUuid(const std::string &uuid) {
  m_highlightUuid = uuid;
}

void Viewer3DController::SetSelectedUuids(
    const std::vector<std::string> &uuids) {
  m_selectedUuids.clear();
  for (const auto &u : uuids)
    m_selectedUuids.insert(u);
}

// Loads meshes or GDTF models referenced by scene objects. Called when the
// scene is updated.
void Viewer3DController::Update() {
  const std::string &base = ConfigManager::Get().GetScene().basePath;

  const auto &trusses = SceneDataManager::Instance().GetTrusses();
  for (const auto &[uuid, t] : trusses) {
    if (t.symbolFile.empty())
      continue;

    std::string path = ResolveModelPath(base, t.symbolFile);
    if (path.empty())
      continue;
    if (m_loadedMeshes.find(path) == m_loadedMeshes.end()) {
      Mesh mesh;
      bool loaded = false;
      std::string ext = fs::path(path).extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if (ext == ".3ds")
        loaded = Load3DS(path, mesh);
      else if (ext == ".glb")
        loaded = LoadGLB(path, mesh);

      if (loaded) {
        m_loadedMeshes[path] = std::move(mesh);
      } else if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format("Failed to load model: %s",
                                        wxString::FromUTF8(path));
        ConsolePanel::Instance()->AppendMessage(msg);
      }
    }
  }

  const auto &objects = SceneDataManager::Instance().GetSceneObjects();
  for (const auto &[uuid, obj] : objects) {
    if (obj.modelFile.empty())
      continue;
    std::string path = ResolveModelPath(base, obj.modelFile);
    if (path.empty())
      continue;
    if (m_loadedMeshes.find(path) == m_loadedMeshes.end()) {
      Mesh mesh;
      bool loaded = false;
      std::string ext = fs::path(path).extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if (ext == ".3ds")
        loaded = Load3DS(path, mesh);
      else if (ext == ".glb")
        loaded = LoadGLB(path, mesh);

      if (loaded) {
        m_loadedMeshes[path] = std::move(mesh);
      } else if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format("Failed to load model: %s",
                                        wxString::FromUTF8(path));
        ConsolePanel::Instance()->AppendMessage(msg);
      }
    }
  }

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();
  for (const auto &[uuid, f] : fixtures) {
    if (f.gdtfSpec.empty())
      continue;
    std::string gdtfPath = ResolveGdtfPath(base, f.gdtfSpec);
    if (gdtfPath.empty()) {
      if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format("GDTF file not found: %s",
                                        wxString::FromUTF8(f.gdtfSpec));
        ConsolePanel::Instance()->AppendMessage(msg);
      }
      continue;
    }
    if (m_loadedGdtf.find(gdtfPath) == m_loadedGdtf.end()) {
      std::vector<GdtfObject> objs;
      if (LoadGdtf(gdtfPath, objs)) {
        m_loadedGdtf[gdtfPath] = std::move(objs);
      } else if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format("Failed to load GDTF: %s",
                                        wxString::FromUTF8(gdtfPath));
        ConsolePanel::Instance()->AppendMessage(msg);
      }
    }
  }

  // Precompute bounding boxes for hover detection
  m_fixtureBounds.clear();
  for (const auto &[uuid, f] : fixtures) {
    Viewer3DController::BoundingBox bb;
    Matrix fix = f.transform;
    fix.o[0] *= RENDER_SCALE;
    fix.o[1] *= RENDER_SCALE;
    fix.o[2] *= RENDER_SCALE;

    bool found = false;
    bb.min = {FLT_MAX, FLT_MAX, FLT_MAX};
    bb.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    std::string gdtfPath = ResolveGdtfPath(base, f.gdtfSpec);
    auto itg = m_loadedGdtf.find(gdtfPath);
    if (itg != m_loadedGdtf.end()) {
      for (const auto &obj : itg->second) {
        for (size_t vi = 0; vi + 2 < obj.mesh.vertices.size(); vi += 3) {
          std::array<float, 3> p = {obj.mesh.vertices[vi] * RENDER_SCALE,
                                    obj.mesh.vertices[vi + 1] * RENDER_SCALE,
                                    obj.mesh.vertices[vi + 2] * RENDER_SCALE};
          p = TransformPoint(obj.transform, p);
          p = TransformPoint(fix, p);
          bb.min[0] = std::min(bb.min[0], p[0]);
          bb.min[1] = std::min(bb.min[1], p[1]);
          bb.min[2] = std::min(bb.min[2], p[2]);
          bb.max[0] = std::max(bb.max[0], p[0]);
          bb.max[1] = std::max(bb.max[1], p[1]);
          bb.max[2] = std::max(bb.max[2], p[2]);
          found = true;
        }
      }
    }

    if (!found) {
      float half = 0.1f;
      std::array<std::array<float, 3>, 8> corners = {
          std::array<float, 3>{-half, -half, -half},
          {half, -half, -half},
          {-half, half, -half},
          {half, half, -half},
          {-half, -half, half},
          {half, -half, half},
          {-half, half, half},
          {half, half, half}};
      for (const auto &c : corners) {
        auto p = TransformPoint(fix, c);
        bb.min[0] = std::min(bb.min[0], p[0]);
        bb.min[1] = std::min(bb.min[1], p[1]);
        bb.min[2] = std::min(bb.min[2], p[2]);
        bb.max[0] = std::max(bb.max[0], p[0]);
        bb.max[1] = std::max(bb.max[1], p[1]);
        bb.max[2] = std::max(bb.max[2], p[2]);
      }
    }

    m_fixtureBounds[uuid] = bb;
  }

  m_trussBounds.clear();
  for (const auto &[uuid, t] : trusses) {
    BoundingBox bb;
    Matrix tm = t.transform;
    tm.o[0] *= RENDER_SCALE;
    tm.o[1] *= RENDER_SCALE;
    tm.o[2] *= RENDER_SCALE;

    bool found = false;
    bb.min = {FLT_MAX, FLT_MAX, FLT_MAX};
    bb.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    if (!t.symbolFile.empty()) {
      std::string path = ResolveModelPath(base, t.symbolFile);
      auto it = m_loadedMeshes.find(path);
      if (it != m_loadedMeshes.end()) {
        for (size_t vi = 0; vi + 2 < it->second.vertices.size(); vi += 3) {
          std::array<float, 3> p = {it->second.vertices[vi] * RENDER_SCALE,
                                    it->second.vertices[vi + 1] * RENDER_SCALE,
                                    it->second.vertices[vi + 2] * RENDER_SCALE};
          p = TransformPoint(tm, p);
          bb.min[0] = std::min(bb.min[0], p[0]);
          bb.min[1] = std::min(bb.min[1], p[1]);
          bb.min[2] = std::min(bb.min[2], p[2]);
          bb.max[0] = std::max(bb.max[0], p[0]);
          bb.max[1] = std::max(bb.max[1], p[1]);
          bb.max[2] = std::max(bb.max[2], p[2]);
          found = true;
        }
      }
    }

    if (!found) {
      float len = (t.lengthMm > 0 ? t.lengthMm * RENDER_SCALE : 0.3f);
      float halfy = (t.widthMm > 0 ? t.widthMm * RENDER_SCALE * 0.5f : 0.15f);
      float z1 = (t.heightMm > 0 ? t.heightMm * RENDER_SCALE : 0.3f);
      std::array<std::array<float, 3>, 8> corners = {
          std::array<float, 3>{0.0f, -halfy, 0.0f},
          {len, -halfy, 0.0f},
          {0.0f, halfy, 0.0f},
          {len, halfy, 0.0f},
          {0.0f, -halfy, z1},
          {len, -halfy, z1},
          {0.0f, halfy, z1},
          {len, halfy, z1}};
      for (const auto &c : corners) {
        auto p = TransformPoint(tm, c);
        bb.min[0] = std::min(bb.min[0], p[0]);
        bb.min[1] = std::min(bb.min[1], p[1]);
        bb.min[2] = std::min(bb.min[2], p[2]);
        bb.max[0] = std::max(bb.max[0], p[0]);
        bb.max[1] = std::max(bb.max[1], p[1]);
        bb.max[2] = std::max(bb.max[2], p[2]);
      }
    }

    m_trussBounds[uuid] = bb;
  }

  m_objectBounds.clear();
  for (const auto &[uuid, obj] : objects) {
    BoundingBox bb;
    Matrix tm = obj.transform;
    tm.o[0] *= RENDER_SCALE;
    tm.o[1] *= RENDER_SCALE;
    tm.o[2] *= RENDER_SCALE;

    bool found = false;
    bb.min = {FLT_MAX, FLT_MAX, FLT_MAX};
    bb.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    if (!obj.modelFile.empty()) {
      std::string path = ResolveModelPath(base, obj.modelFile);
      auto it = m_loadedMeshes.find(path);
      if (it != m_loadedMeshes.end()) {
        for (size_t vi = 0; vi + 2 < it->second.vertices.size(); vi += 3) {
          std::array<float, 3> p = {it->second.vertices[vi] * RENDER_SCALE,
                                    it->second.vertices[vi + 1] * RENDER_SCALE,
                                    it->second.vertices[vi + 2] * RENDER_SCALE};
          p = TransformPoint(tm, p);
          bb.min[0] = std::min(bb.min[0], p[0]);
          bb.min[1] = std::min(bb.min[1], p[1]);
          bb.min[2] = std::min(bb.min[2], p[2]);
          bb.max[0] = std::max(bb.max[0], p[0]);
          bb.max[1] = std::max(bb.max[1], p[1]);
          bb.max[2] = std::max(bb.max[2], p[2]);
          found = true;
        }
      }
    }

    if (!found) {
      float half = 0.15f;
      std::array<std::array<float, 3>, 8> corners = {
          std::array<float, 3>{-half, -half, -half},
          {half, -half, -half},
          {-half, half, -half},
          {half, half, -half},
          {-half, -half, half},
          {half, -half, half},
          {-half, half, half},
          {half, half, half}};
      for (const auto &c : corners) {
        auto p = TransformPoint(tm, c);
        bb.min[0] = std::min(bb.min[0], p[0]);
        bb.min[1] = std::min(bb.min[1], p[1]);
        bb.min[2] = std::min(bb.min[2], p[2]);
        bb.max[0] = std::max(bb.max[0], p[0]);
        bb.max[1] = std::max(bb.max[1], p[1]);
        bb.max[2] = std::max(bb.max[2], p[2]);
      }
    }

    m_objectBounds[uuid] = bb;
  }
}

// Renders all scene objects using their transformMatrix
void Viewer3DController::RenderScene() {
  SetupBasicLighting();
  DrawGrid();

  // Fixtures
  glShadeModel(GL_FLAT);
  const auto &fixtures = SceneDataManager::Instance().GetFixtures();
  const std::string &base = ConfigManager::Get().GetScene().basePath;
  for (const auto &[uuid, f] : fixtures) {
    if (!ConfigManager::Get().IsLayerVisible(f.layer))
      continue;
    glPushMatrix();

    bool highlight = (!m_highlightUuid.empty() && uuid == m_highlightUuid);
    bool selected = (m_selectedUuids.find(uuid) != m_selectedUuids.end());

    float matrix[16];
    MatrixToArray(f.transform, matrix);
    ApplyTransform(matrix, true);

    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    auto fbit = m_fixtureBounds.find(uuid);
    if (fbit != m_fixtureBounds.end()) {
      cx = (fbit->second.min[0] + fbit->second.max[0]) * 0.5f;
      cy = (fbit->second.min[1] + fbit->second.max[1]) * 0.5f;
      cz = (fbit->second.min[2] + fbit->second.max[2]) * 0.5f;
      cx -= f.transform.o[0] * RENDER_SCALE;
      cy -= f.transform.o[1] * RENDER_SCALE;
      cz -= f.transform.o[2] * RENDER_SCALE;
    }

    std::string gdtfPath = ResolveGdtfPath(base, f.gdtfSpec);
    auto itg = m_loadedGdtf.find(gdtfPath);

    if (itg != m_loadedGdtf.end()) {
      for (const auto &obj : itg->second) {
        glPushMatrix();
        float m2[16];
        MatrixToArray(obj.transform, m2);
        // GDTF geometry offsets are defined relative to the fixture
        // in meters. Only the vertex coordinates need unit scaling.
        ApplyTransform(m2, false);
        DrawMeshWithOutline(obj.mesh, 1.0f, 1.0f, 1.0f, RENDER_SCALE, highlight,
                            selected, cx, cy, cz);
        glPopMatrix();
      }
    } else {
      DrawCubeWithOutline(0.2f, 0.8f, 0.8f, 1.0f, highlight, selected, cx, cy,
                          cz);
    }

    glPopMatrix();
  }

  glShadeModel(GL_SMOOTH); // keep smooth shading for trusses

  // Trusses
  const auto &trusses = SceneDataManager::Instance().GetTrusses();
  for (const auto &[uuid, t] : trusses) {
    if (!ConfigManager::Get().IsLayerVisible(t.layer))
      continue;
    glPushMatrix();

    bool highlight = (!m_highlightUuid.empty() && uuid == m_highlightUuid);
    bool selected = (m_selectedUuids.find(uuid) != m_selectedUuids.end());

    float matrix[16];
    MatrixToArray(t.transform, matrix);
    ApplyTransform(matrix, true);

    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    auto tbit = m_trussBounds.find(uuid);
    if (tbit != m_trussBounds.end()) {
      cx = (tbit->second.min[0] + tbit->second.max[0]) * 0.5f;
      cy = (tbit->second.min[1] + tbit->second.max[1]) * 0.5f;
      cz = (tbit->second.min[2] + tbit->second.max[2]) * 0.5f;
      cx -= t.transform.o[0] * RENDER_SCALE;
      cy -= t.transform.o[1] * RENDER_SCALE;
      cz -= t.transform.o[2] * RENDER_SCALE;
    }

    if (!t.symbolFile.empty()) {
      std::string path = ResolveModelPath(base, t.symbolFile);
      if (!path.empty()) {
        auto it = m_loadedMeshes.find(path);
        if (it != m_loadedMeshes.end()) {
          DrawMeshWithOutline(it->second, 1.0f, 1.0f, 1.0f, RENDER_SCALE,
                              highlight, selected, cx, cy, cz);
        } else {
          float len = t.lengthMm * RENDER_SCALE;
          float wid = (t.widthMm > 0 ? t.widthMm : 400.0f) * RENDER_SCALE;
          float hei = (t.heightMm > 0 ? t.heightMm : 400.0f) * RENDER_SCALE;
          DrawWireframeBox(len, hei, wid, highlight, selected);
        }
      } else {
        float len = t.lengthMm * RENDER_SCALE;
        float wid = (t.widthMm > 0 ? t.widthMm : 400.0f) * RENDER_SCALE;
        float hei = (t.heightMm > 0 ? t.heightMm : 400.0f) * RENDER_SCALE;
        DrawWireframeBox(len, hei, wid, highlight, selected);
      }
    } else {
      float len = t.lengthMm * RENDER_SCALE;
      float wid = (t.widthMm > 0 ? t.widthMm : 400.0f) * RENDER_SCALE;
      float hei = (t.heightMm > 0 ? t.heightMm : 400.0f) * RENDER_SCALE;
      DrawWireframeBox(len, hei, wid, highlight, selected);
    }

    glPopMatrix();
  }

  glShadeModel(GL_FLAT);

  const auto &meshes = SceneDataManager::Instance().GetSceneObjects();
  for (const auto &[uuid, m] : meshes) {
    if (!ConfigManager::Get().IsLayerVisible(m.layer))
      continue;
    glPushMatrix();

    bool highlight = (!m_highlightUuid.empty() && uuid == m_highlightUuid);
    bool selected = (m_selectedUuids.find(uuid) != m_selectedUuids.end());

    float matrix[16];
    MatrixToArray(m.transform, matrix);
    ApplyTransform(matrix, true);

    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    auto obit = m_objectBounds.find(uuid);
    if (obit != m_objectBounds.end()) {
      cx = (obit->second.min[0] + obit->second.max[0]) * 0.5f;
      cy = (obit->second.min[1] + obit->second.max[1]) * 0.5f;
      cz = (obit->second.min[2] + obit->second.max[2]) * 0.5f;
      cx -= m.transform.o[0] * RENDER_SCALE;
      cy -= m.transform.o[1] * RENDER_SCALE;
      cz -= m.transform.o[2] * RENDER_SCALE;
    }

    if (!m.modelFile.empty()) {
      std::string path = ResolveModelPath(base, m.modelFile);
      if (!path.empty()) {
        auto it = m_loadedMeshes.find(path);
        if (it != m_loadedMeshes.end())
          DrawMeshWithOutline(it->second, 1.0f, 1.0f, 1.0f, RENDER_SCALE,
                              highlight, selected, cx, cy, cz);
        else
          DrawCubeWithOutline(0.3f, 0.8f, 0.8f, 0.8f, highlight, selected, cx,
                              cy, cz);
      } else {
        DrawCubeWithOutline(0.3f, 0.8f, 0.8f, 0.8f, highlight, selected, cx, cy,
                            cz);
      }
    } else {
      DrawCubeWithOutline(0.3f, 0.8f, 0.8f, 0.8f, highlight, selected, cx, cy,
                          cz);
    }

    glPopMatrix();
  }

  const auto &groups = SceneDataManager::Instance().GetGroupObjects();
  for (const auto &[uuid, g] : groups) {
    (void)uuid;
    (void)g; // groups not implemented
  }

  DrawAxes();
}

// Draws a solid cube centered at origin with given size and color
void Viewer3DController::DrawCube(float size, float r, float g, float b) {
  float half = size / 2.0f;
  float x0 = -half, x1 = half;
  float y0 = -half, y1 = half;
  float z0 = -half, z1 = half;

  glColor3f(r, g, b);
  glBegin(GL_QUADS);
  glNormal3f(0.0f, 0.0f, 1.0f);
  glVertex3f(x0, y0, z1);
  glVertex3f(x1, y0, z1);
  glVertex3f(x1, y1, z1);
  glVertex3f(x0, y1, z1); // Front
  glNormal3f(0.0f, 0.0f, -1.0f);
  glVertex3f(x1, y0, z0);
  glVertex3f(x0, y0, z0);
  glVertex3f(x0, y1, z0);
  glVertex3f(x1, y1, z0); // Back
  glNormal3f(-1.0f, 0.0f, 0.0f);
  glVertex3f(x0, y0, z0);
  glVertex3f(x0, y0, z1);
  glVertex3f(x0, y1, z1);
  glVertex3f(x0, y1, z0); // Left
  glNormal3f(1.0f, 0.0f, 0.0f);
  glVertex3f(x1, y0, z1);
  glVertex3f(x1, y0, z0);
  glVertex3f(x1, y1, z0);
  glVertex3f(x1, y1, z1); // Right
  glNormal3f(0.0f, 1.0f, 0.0f);
  glVertex3f(x0, y1, z1);
  glVertex3f(x1, y1, z1);
  glVertex3f(x1, y1, z0);
  glVertex3f(x0, y1, z0); // Top
  glNormal3f(0.0f, -1.0f, 0.0f);
  glVertex3f(x0, y0, z0);
  glVertex3f(x1, y0, z0);
  glVertex3f(x1, y0, z1);
  glVertex3f(x0, y0, z1); // Bottom
  glEnd();
}

// Draws a wireframe cube centered at origin with given size
void Viewer3DController::DrawWireframeCube(float size) {
  float half = size / 2.0f;
  float x0 = -half, x1 = half;
  float y0 = -half, y1 = half;
  float z0 = -half, z1 = half;

  glColor3f(1.0f, 1.0f, 0.0f);
  glBegin(GL_LINES);
  glVertex3f(x0, y0, z0);
  glVertex3f(x1, y0, z0);
  glVertex3f(x0, y1, z0);
  glVertex3f(x1, y1, z0);
  glVertex3f(x0, y0, z1);
  glVertex3f(x1, y0, z1);
  glVertex3f(x0, y1, z1);
  glVertex3f(x1, y1, z1);
  glVertex3f(x0, y0, z0);
  glVertex3f(x0, y1, z0);
  glVertex3f(x1, y0, z0);
  glVertex3f(x1, y1, z0);
  glVertex3f(x0, y0, z1);
  glVertex3f(x0, y1, z1);
  glVertex3f(x1, y0, z1);
  glVertex3f(x1, y1, z1);
  glVertex3f(x0, y0, z0);
  glVertex3f(x0, y0, z1);
  glVertex3f(x1, y0, z0);
  glVertex3f(x1, y0, z1);
  glVertex3f(x0, y1, z0);
  glVertex3f(x0, y1, z1);
  glVertex3f(x1, y1, z0);
  glVertex3f(x1, y1, z1);
  glEnd();
}

// Draws a wireframe box whose origin sits at the left end of the span.
// The box extends along +X for the given length and is centered in Y/Z.
void Viewer3DController::DrawWireframeBox(float length, float height,
                                          float width, bool highlight,
                                          bool selected) {
  float x0 = 0.0f, x1 = length;
  float y0 = -width * 0.5f, y1 = width * 0.5f;
  float z0 = 0.0f, z1 = height;

  if (selected)
    glColor3f(0.0f, 1.0f, 1.0f);
  else if (highlight)
    glColor3f(0.0f, 1.0f, 0.0f);
  else
    glColor3f(1.0f, 1.0f, 0.0f);

  glBegin(GL_LINES);
  glVertex3f(x0, y0, z0);
  glVertex3f(x1, y0, z0);
  glVertex3f(x0, y1, z0);
  glVertex3f(x1, y1, z0);
  glVertex3f(x0, y0, z1);
  glVertex3f(x1, y0, z1);
  glVertex3f(x0, y1, z1);
  glVertex3f(x1, y1, z1);
  glVertex3f(x0, y0, z0);
  glVertex3f(x0, y1, z0);
  glVertex3f(x1, y0, z0);
  glVertex3f(x1, y1, z0);
  glVertex3f(x0, y0, z1);
  glVertex3f(x0, y1, z1);
  glVertex3f(x1, y0, z1);
  glVertex3f(x1, y1, z1);
  glVertex3f(x0, y0, z0);
  glVertex3f(x0, y0, z1);
  glVertex3f(x1, y0, z0);
  glVertex3f(x1, y0, z1);
  glVertex3f(x0, y1, z0);
  glVertex3f(x0, y1, z1);
  glVertex3f(x1, y1, z0);
  glVertex3f(x1, y1, z1);
  glEnd();
}

// Draws a colored cube. If selected or highlighted it is tinted
// in cyan or green respectively instead of its original color.
void Viewer3DController::DrawCubeWithOutline(float size, float r, float g,
                                             float b, bool highlight,
                                             bool selected, float cx, float cy,
                                             float cz) {
  (void)cx;
  (void)cy;
  (void)cz; // parameters no longer used

  if (selected)
    DrawCube(size, 0.0f, 1.0f, 1.0f);
  else if (highlight)
    DrawCube(size, 0.0f, 1.0f, 0.0f);
  else
    DrawCube(size, r, g, b);
}

// Draws a mesh using the given color. When selected or highlighted the
// mesh is rendered entirely in cyan or green respectively.
void Viewer3DController::DrawMeshWithOutline(const Mesh &mesh, float r, float g,
                                             float b, float scale,
                                             bool highlight, bool selected,
                                             float cx, float cy, float cz) {
  (void)cx;
  (void)cy;
  (void)cz; // parameters kept for compatibility

  if (selected)
    glColor3f(0.0f, 1.0f, 1.0f);
  else if (highlight)
    glColor3f(0.0f, 1.0f, 0.0f);
  else
    glColor3f(r, g, b);

  DrawMesh(mesh, scale);
}

// Draws a mesh using GL triangles. The optional scale parameter allows
// converting vertex units (e.g. millimeters) to meters.
void Viewer3DController::DrawMesh(const Mesh &mesh, float scale) {
  glBegin(GL_TRIANGLES);
  bool hasNormals = mesh.normals.size() >= mesh.vertices.size();
  for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    unsigned short i0 = mesh.indices[i];
    unsigned short i1 = mesh.indices[i + 1];
    unsigned short i2 = mesh.indices[i + 2];

    float v0x = mesh.vertices[i0 * 3] * scale;
    float v0y = mesh.vertices[i0 * 3 + 1] * scale;
    float v0z = mesh.vertices[i0 * 3 + 2] * scale;

    float v1x = mesh.vertices[i1 * 3] * scale;
    float v1y = mesh.vertices[i1 * 3 + 1] * scale;
    float v1z = mesh.vertices[i1 * 3 + 2] * scale;

    float v2x = mesh.vertices[i2 * 3] * scale;
    float v2y = mesh.vertices[i2 * 3 + 1] * scale;
    float v2z = mesh.vertices[i2 * 3 + 2] * scale;

    if (hasNormals) {
      glNormal3f(mesh.normals[i0 * 3], mesh.normals[i0 * 3 + 1],
                 mesh.normals[i0 * 3 + 2]);
      glVertex3f(v0x, v0y, v0z);
      glNormal3f(mesh.normals[i1 * 3], mesh.normals[i1 * 3 + 1],
                 mesh.normals[i1 * 3 + 2]);
      glVertex3f(v1x, v1y, v1z);
      glNormal3f(mesh.normals[i2 * 3], mesh.normals[i2 * 3 + 1],
                 mesh.normals[i2 * 3 + 2]);
      glVertex3f(v2x, v2y, v2z);
    } else {
      float ux = v1x - v0x;
      float uy = v1y - v0y;
      float uz = v1z - v0z;

      float vx = v2x - v0x;
      float vy = v2y - v0y;
      float vz = v2z - v0z;

      float nx = uy * vz - uz * vy;
      float ny = uz * vx - ux * vz;
      float nz = ux * vy - uy * vx;

      float len = std::sqrt(nx * nx + ny * ny + nz * nz);
      if (len > 0.0f) {
        nx /= len;
        ny /= len;
        nz /= len;
      }

      glNormal3f(nx, ny, nz);
      glVertex3f(v0x, v0y, v0z);
      glVertex3f(v1x, v1y, v1z);
      glVertex3f(v2x, v2y, v2z);
    }
  }
  glEnd();
}

// Draws the ground grid on the Z=0 plane
void Viewer3DController::DrawGrid() {
  const float size = 20.0f;
  const float step = 1.0f;

  glColor3f(0.35f, 0.35f, 0.35f);
  glLineWidth(1.0f);
  glBegin(GL_LINES);
  for (float i = -size; i <= size; i += step) {
    glVertex3f(i, -size, 0.0f);
    glVertex3f(i, size, 0.0f);
    glVertex3f(-size, i, 0.0f);
    glVertex3f(size, i, 0.0f);
  }
  glEnd();
}

// Draws the XYZ axes centered at origin
void Viewer3DController::DrawAxes() {
  glLineWidth(2.0f);
  glBegin(GL_LINES);
  glColor3f(1.0f, 0.0f, 0.0f);
  glVertex3f(0.0f, 0.0f, 0.0f);
  glVertex3f(1.0f, 0.0f, 0.0f); // X
  glColor3f(0.0f, 1.0f, 0.0f);
  glVertex3f(0.0f, 0.0f, 0.0f);
  glVertex3f(0.0f, 1.0f, 0.0f); // Y
  glColor3f(0.0f, 0.0f, 1.0f);
  glVertex3f(0.0f, 0.0f, 0.0f);
  glVertex3f(0.0f, 0.0f, 1.0f); // Z
  glEnd();
}

// Multiplies the current matrix by the given transform. When
// scaleTranslation is true the translation part is converted from
// millimeters to meters using RENDER_SCALE.
void Viewer3DController::ApplyTransform(const float matrix[16],
                                        bool scaleTranslation) {
  float m[16];
  std::copy(matrix, matrix + 16, m);
  if (scaleTranslation) {
    m[12] *= RENDER_SCALE;
    m[13] *= RENDER_SCALE;
    m[14] *= RENDER_SCALE;
  }
  glMultMatrixf(m);
}

// Initializes simple lighting for the scene
void Viewer3DController::SetupBasicLighting() {
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);

  GLfloat ambient[] = {0.2f, 0.2f, 0.2f, 1.0f};
  GLfloat diffuse[] = {0.8f, 0.8f, 0.8f, 1.0f};
  GLfloat specular[] = {1.0f, 1.0f, 1.0f, 1.0f};
  GLfloat position[] = {2.0f, -4.0f, 5.0f, 0.0f};

  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
  glLightfv(GL_LIGHT0, GL_POSITION, position);

  glEnable(GL_COLOR_MATERIAL);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  glShadeModel(GL_SMOOTH);
}

void Viewer3DController::SetupMaterialFromRGB(float r, float g, float b) {
  glColor3f(r, g, b);
}

void Viewer3DController::DrawFixtureLabels(int width, int height) {
  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();
  for (const auto &[uuid, f] : fixtures) {
    if (!ConfigManager::Get().IsLayerVisible(f.layer))
      continue;
    if (uuid != m_highlightUuid)
      continue;

    double sx, sy, sz;
    // Use bounding box center if available
    auto bit = m_fixtureBounds.find(uuid);
    if (bit != m_fixtureBounds.end()) {
      const BoundingBox &bb = bit->second;
      double wx = (bb.min[0] + bb.max[0]) * 0.5;
      double wy = (bb.min[1] + bb.max[1]) * 0.5;
      double wz = (bb.min[2] + bb.max[2]) * 0.5;
      if (gluProject(wx, wy, wz, model, proj, viewport, &sx, &sy, &sz) !=
          GL_TRUE)
        continue;
    } else {
      double wx = f.transform.o[0] * RENDER_SCALE;
      double wy = f.transform.o[1] * RENDER_SCALE;
      double wz = f.transform.o[2] * RENDER_SCALE;
      if (gluProject(wx, wy, wz, model, proj, viewport, &sx, &sy, &sz) !=
          GL_TRUE)
        continue;
    }

    int x = static_cast<int>(sx);
    int y = height - static_cast<int>(sy);
    wxString label = f.instanceName.empty()
                         ? wxString::FromUTF8(uuid)
                         : wxString::FromUTF8(f.instanceName);
    label += "\nID: " + wxString::Format("%d", f.fixtureId);
    if (!f.address.empty())
      label += "\n" + wxString::FromUTF8(f.address);

    auto utf8 = label.ToUTF8();
    DrawText2D(m_vg, m_font, std::string(utf8.data(), utf8.length()), x, y);
  }
}

// Renders labels for all fixtures in the current scene. Each label displays the
// fixture's instance name (or UUID), numeric ID and DMX address. Labels are
// placed slightly below the fixture's bounding box so they appear attached to
// the bottom of the fixture in the 2D top-down view. The label width derives
// from the fixture bounds so the drawn tag roughly matches the fixture size,
// and the optional zoom parameter scales the label like regular geometry when
// zooming the 2D view.
void Viewer3DController::DrawAllFixtureLabels(int width, int height,
                                              float zoom) {
  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();
  for (const auto &[uuid, f] : fixtures) {
    if (!ConfigManager::Get().IsLayerVisible(f.layer))
      continue;

    double wx, wy, wz;
    auto bit = m_fixtureBounds.find(uuid);
    if (bit != m_fixtureBounds.end()) {
      const BoundingBox &bb = bit->second;
      wx = (bb.min[0] + bb.max[0]) * 0.5;
      wy = bb.min[1];
      wz = (bb.min[2] + bb.max[2]) * 0.5;
    } else {
      wx = f.transform.o[0] * RENDER_SCALE;
      wy = f.transform.o[1] * RENDER_SCALE;
      wz = f.transform.o[2] * RENDER_SCALE;
    }

    double sx, sy, sz;
    if (gluProject(wx, wy, wz, model, proj, viewport, &sx, &sy, &sz) != GL_TRUE)
      continue;

    int x = static_cast<int>(sx);
    // Convert OpenGL's origin to top-left and move a few pixels downward so the
    // label sits just below the fixture.
    int y = height - static_cast<int>(sy) + 10;

    wxString baseName = f.instanceName.empty()
                            ? wxString::FromUTF8(uuid)
                            : wxString::FromUTF8(f.instanceName);
    wxString label = WrapEveryTwoWords(baseName);
    label += "\nID: " + wxString::Format("%d", f.fixtureId);
    wxString addr =
        f.address.empty() ? wxString() : wxString::FromUTF8(f.address);
    label += "\n" + addr;

    auto utf8 = label.ToUTF8();
    float fontSize = LABEL_FONT_SIZE_2D * zoom;
    DrawText2D(m_vg, m_font, std::string(utf8.data(), utf8.length()), x, y,
               fontSize, 0.0f, false);
  }
}

bool Viewer3DController::GetFixtureLabelAt(int mouseX, int mouseY, int width,
                                           int height, wxString &outLabel,
                                           wxPoint &outPos,
                                           std::string *outUuid) {
  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();

  bool found = false;
  double bestDepth = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;
  std::string bestUuid;

  for (const auto &[uuid, f] : fixtures) {
    if (!ConfigManager::Get().IsLayerVisible(f.layer))
      continue;
    auto bit = m_fixtureBounds.find(uuid);
    if (bit == m_fixtureBounds.end())
      continue;

    const BoundingBox &bb = bit->second;
    std::array<std::array<float, 3>, 8> corners = {
        std::array<float, 3>{bb.min[0], bb.min[1], bb.min[2]},
        {bb.max[0], bb.min[1], bb.min[2]},
        {bb.min[0], bb.max[1], bb.min[2]},
        {bb.max[0], bb.max[1], bb.min[2]},
        {bb.min[0], bb.min[1], bb.max[2]},
        {bb.max[0], bb.min[1], bb.max[2]},
        {bb.min[0], bb.max[1], bb.max[2]},
        {bb.max[0], bb.max[1], bb.max[2]}};

    ScreenRect rect;
    double minDepth = DBL_MAX;
    bool visible = false;
    for (const auto &c : corners) {
      double sx, sy, sz;
      if (gluProject(c[0], c[1], c[2], model, proj, viewport, &sx, &sy, &sz) ==
          GL_TRUE) {
        rect.minX = std::min(rect.minX, sx);
        rect.maxX = std::max(rect.maxX, sx);
        double sy2 = height - sy;
        rect.minY = std::min(rect.minY, sy2);
        rect.maxY = std::max(rect.maxY, sy2);
        if (sz >= 0.0 && sz <= 1.0) {
          visible = true;
          minDepth = std::min(minDepth, sz);
        }
      }
    }

    if (!visible)
      continue;

    if (mouseX >= rect.minX && mouseX <= rect.maxX && mouseY >= rect.minY &&
        mouseY <= rect.maxY) {
      if (minDepth < bestDepth) {
        bestDepth = minDepth;
        bestPos.x = static_cast<int>((rect.minX + rect.maxX) * 0.5);
        bestPos.y = static_cast<int>((rect.minY + rect.maxY) * 0.5);
        bestLabel = f.instanceName.empty() ? wxString::FromUTF8(uuid)
                                           : wxString::FromUTF8(f.instanceName);
        bestLabel += "\nID: " + wxString::Format("%d", f.fixtureId);
        wxString addr =
            f.address.empty() ? wxString() : wxString::FromUTF8(f.address);
        bestLabel += "\n" + addr;
        bestUuid = uuid;
        found = true;
      }
    }
  }

  if (found) {
    outPos = bestPos;
    outLabel = bestLabel;
    if (outUuid)
      *outUuid = bestUuid;
  }
  return found;
}

void Viewer3DController::DrawTrussLabels(int width, int height) {

  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const auto &trusses = SceneDataManager::Instance().GetTrusses();
  for (const auto &[uuid, t] : trusses) {
    if (!ConfigManager::Get().IsLayerVisible(t.layer))
      continue;
    if (uuid != m_highlightUuid)
      continue;

    double sx, sy, sz;
    auto bit = m_trussBounds.find(uuid);
    if (bit != m_trussBounds.end()) {
      const BoundingBox &bb = bit->second;
      double wx = (bb.min[0] + bb.max[0]) * 0.5;
      double wy = (bb.min[1] + bb.max[1]) * 0.5;
      double wz = (bb.min[2] + bb.max[2]) * 0.5;
      if (gluProject(wx, wy, wz, model, proj, viewport, &sx, &sy, &sz) !=
          GL_TRUE)
        continue;
    } else {
      double wx = t.transform.o[0] * RENDER_SCALE;
      double wy = t.transform.o[1] * RENDER_SCALE;
      double wz = t.transform.o[2] * RENDER_SCALE;
      if (gluProject(wx, wy, wz, model, proj, viewport, &sx, &sy, &sz) !=
          GL_TRUE)
        continue;
    }

    int x = static_cast<int>(sx);
    int y = height - static_cast<int>(sy);
    wxString label =
        t.name.empty() ? wxString::FromUTF8(uuid) : wxString::FromUTF8(t.name);
    float baseHeight = t.transform.o[2] - t.heightMm * 0.5f;
    std::string hStr = FormatMeters(baseHeight);
    label += wxString::Format("\nh = %s m", hStr.c_str());

    auto utf8 = label.ToUTF8();
    DrawText2D(m_vg, m_font, std::string(utf8.data(), utf8.length()), x, y);
  }
}

void Viewer3DController::DrawSceneObjectLabels(int width, int height) {

  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const auto &objs = SceneDataManager::Instance().GetSceneObjects();
  for (const auto &[uuid, o] : objs) {
    if (!ConfigManager::Get().IsLayerVisible(o.layer))
      continue;
    if (uuid != m_highlightUuid)
      continue;

    double sx, sy, sz;
    auto bit = m_objectBounds.find(uuid);
    if (bit != m_objectBounds.end()) {
      const BoundingBox &bb = bit->second;
      double wx = (bb.min[0] + bb.max[0]) * 0.5;
      double wy = (bb.min[1] + bb.max[1]) * 0.5;
      double wz = (bb.min[2] + bb.max[2]) * 0.5;
      if (gluProject(wx, wy, wz, model, proj, viewport, &sx, &sy, &sz) !=
          GL_TRUE)
        continue;
    } else {
      double wx = o.transform.o[0] * RENDER_SCALE;
      double wy = o.transform.o[1] * RENDER_SCALE;
      double wz = o.transform.o[2] * RENDER_SCALE;
      if (gluProject(wx, wy, wz, model, proj, viewport, &sx, &sy, &sz) !=
          GL_TRUE)
        continue;
    }

    int x = static_cast<int>(sx);
    int y = height - static_cast<int>(sy);
    wxString label =
        o.name.empty() ? wxString::FromUTF8(uuid) : wxString::FromUTF8(o.name);

    auto utf8 = label.ToUTF8();
    DrawText2D(m_vg, m_font, std::string(utf8.data(), utf8.length()), x, y);
  }
}

bool Viewer3DController::GetTrussLabelAt(int mouseX, int mouseY, int width,
                                         int height, wxString &outLabel,
                                         wxPoint &outPos,
                                         std::string *outUuid) {
  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const auto &trusses = SceneDataManager::Instance().GetTrusses();
  bool found = false;
  double bestDepth = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;
  std::string bestUuid;
  for (const auto &[uuid, t] : trusses) {
    if (!ConfigManager::Get().IsLayerVisible(t.layer))
      continue;
    auto bit = m_trussBounds.find(uuid);
    if (bit == m_trussBounds.end())
      continue;

    const BoundingBox &bb = bit->second;
    std::array<std::array<float, 3>, 8> corners = {
        std::array<float, 3>{bb.min[0], bb.min[1], bb.min[2]},
        {bb.max[0], bb.min[1], bb.min[2]},
        {bb.min[0], bb.max[1], bb.min[2]},
        {bb.max[0], bb.max[1], bb.min[2]},
        {bb.min[0], bb.min[1], bb.max[2]},
        {bb.max[0], bb.min[1], bb.max[2]},
        {bb.min[0], bb.max[1], bb.max[2]},
        {bb.max[0], bb.max[1], bb.max[2]}};

    ScreenRect rect;
    double minDepth = DBL_MAX;
    bool visible = false;
    for (const auto &c : corners) {
      double sx, sy, sz;
      if (gluProject(c[0], c[1], c[2], model, proj, viewport, &sx, &sy, &sz) ==
          GL_TRUE) {
        rect.minX = std::min(rect.minX, sx);
        rect.maxX = std::max(rect.maxX, sx);
        double sy2 = height - sy;
        rect.minY = std::min(rect.minY, sy2);
        rect.maxY = std::max(rect.maxY, sy2);
        if (sz >= 0.0 && sz <= 1.0) {
          visible = true;
          minDepth = std::min(minDepth, sz);
        }
      }
    }

    if (!visible)
      continue;

    if (mouseX >= rect.minX && mouseX <= rect.maxX && mouseY >= rect.minY &&
        mouseY <= rect.maxY) {
      if (minDepth < bestDepth) {
        bestDepth = minDepth;
        bestPos.x = static_cast<int>((rect.minX + rect.maxX) * 0.5);
        bestPos.y = static_cast<int>((rect.minY + rect.maxY) * 0.5);
        bestLabel = t.name.empty() ? wxString::FromUTF8(uuid)
                                   : wxString::FromUTF8(t.name);
        float baseHeight = t.transform.o[2] - t.heightMm * 0.5f;
        std::string hStr = FormatMeters(baseHeight);
        bestLabel += wxString::Format("\nh = %s m", hStr.c_str());
        bestUuid = uuid;
        found = true;
      }
    }
  }

  if (found) {
    outPos = bestPos;
    outLabel = bestLabel;
    if (outUuid)
      *outUuid = bestUuid;
  }
  return found;
}

bool Viewer3DController::GetSceneObjectLabelAt(int mouseX, int mouseY,
                                               int width, int height,
                                               wxString &outLabel,
                                               wxPoint &outPos,
                                               std::string *outUuid) {
  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const auto &objs = SceneDataManager::Instance().GetSceneObjects();
  bool found = false;
  double bestDepth = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;
  std::string bestUuid;
  for (const auto &[uuid, o] : objs) {
    if (!ConfigManager::Get().IsLayerVisible(o.layer))
      continue;
    auto bit = m_objectBounds.find(uuid);
    if (bit == m_objectBounds.end())
      continue;

    const BoundingBox &bb = bit->second;
    std::array<std::array<float, 3>, 8> corners = {
        std::array<float, 3>{bb.min[0], bb.min[1], bb.min[2]},
        {bb.max[0], bb.min[1], bb.min[2]},
        {bb.min[0], bb.max[1], bb.min[2]},
        {bb.max[0], bb.max[1], bb.min[2]},
        {bb.min[0], bb.min[1], bb.max[2]},
        {bb.max[0], bb.min[1], bb.max[2]},
        {bb.min[0], bb.max[1], bb.max[2]},
        {bb.max[0], bb.max[1], bb.max[2]}};

    ScreenRect rect;
    double minDepth = DBL_MAX;
    bool visible = false;
    for (const auto &c : corners) {
      double sx, sy, sz;
      if (gluProject(c[0], c[1], c[2], model, proj, viewport, &sx, &sy, &sz) ==
          GL_TRUE) {
        rect.minX = std::min(rect.minX, sx);
        rect.maxX = std::max(rect.maxX, sx);
        double sy2 = height - sy;
        rect.minY = std::min(rect.minY, sy2);
        rect.maxY = std::max(rect.maxY, sy2);
        if (sz >= 0.0 && sz <= 1.0) {
          visible = true;
          minDepth = std::min(minDepth, sz);
        }
      }
    }

    if (!visible)
      continue;

    if (mouseX >= rect.minX && mouseX <= rect.maxX && mouseY >= rect.minY &&
        mouseY <= rect.maxY) {
      if (minDepth < bestDepth) {
        bestDepth = minDepth;
        bestPos.x = static_cast<int>((rect.minX + rect.maxX) * 0.5);
        bestPos.y = static_cast<int>((rect.minY + rect.maxY) * 0.5);
        bestLabel = o.name.empty() ? wxString::FromUTF8(uuid)
                                   : wxString::FromUTF8(o.name);
        bestUuid = uuid;
        found = true;
      }
    }
  }

  if (found) {
    outPos = bestPos;
    outLabel = bestLabel;
    if (outUuid)
      *outUuid = bestUuid;
  }
  return found;
}
