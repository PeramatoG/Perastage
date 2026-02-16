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
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <wx/wx.h>

#include "fixturepreviewpanel.h"

#include "matrixutils.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

static constexpr float RENDER_SCALE = 0.001f;

static std::array<float, 3> TransformPoint(const Matrix &m,
                                           const std::array<float, 3> &p) {
  return {m.u[0] * p[0] + m.v[0] * p[1] + m.w[0] * p[2] + m.o[0],
          m.u[1] * p[0] + m.v[1] * p[1] + m.w[1] * p[2] + m.o[1],
          m.u[2] * p[0] + m.v[2] * p[1] + m.w[2] * p[2] + m.o[2]};
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

static void DrawCube(float size) {
  float h = size * 0.5f;
  glBegin(GL_QUADS);
  glVertex3f(-h, -h, h);
  glVertex3f(h, -h, h);
  glVertex3f(h, h, h);
  glVertex3f(-h, h, h);
  glVertex3f(-h, -h, -h);
  glVertex3f(-h, h, -h);
  glVertex3f(h, h, -h);
  glVertex3f(h, -h, -h);
  glVertex3f(-h, -h, -h);
  glVertex3f(-h, -h, h);
  glVertex3f(-h, h, h);
  glVertex3f(-h, h, -h);
  glVertex3f(h, -h, -h);
  glVertex3f(h, h, -h);
  glVertex3f(h, h, h);
  glVertex3f(h, -h, h);
  glVertex3f(-h, h, h);
  glVertex3f(h, h, h);
  glVertex3f(h, h, -h);
  glVertex3f(-h, h, -h);
  glVertex3f(-h, -h, h);
  glVertex3f(-h, -h, -h);
  glVertex3f(h, -h, -h);
  glVertex3f(h, -h, h);
  glEnd();
}

static void DrawMesh(const Mesh &mesh, float scale) {
  glBegin(GL_TRIANGLES);
  bool hasNormals = mesh.normals.size() >= mesh.vertices.size();
  for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    unsigned short i0 = mesh.indices[i];
    unsigned short i1 = mesh.indices[i + 1];
    unsigned short i2 = mesh.indices[i + 2];
    const float v[9] = {
        mesh.vertices[i0 * 3] * scale, mesh.vertices[i0 * 3 + 1] * scale,
        mesh.vertices[i0 * 3 + 2] * scale, mesh.vertices[i1 * 3] * scale,
        mesh.vertices[i1 * 3 + 1] * scale, mesh.vertices[i1 * 3 + 2] * scale,
        mesh.vertices[i2 * 3] * scale, mesh.vertices[i2 * 3 + 1] * scale,
        mesh.vertices[i2 * 3 + 2] * scale};
    if (hasNormals) {
      glNormal3f(mesh.normals[i0 * 3], mesh.normals[i0 * 3 + 1],
                 mesh.normals[i0 * 3 + 2]);
      glVertex3f(v[0], v[1], v[2]);
      glNormal3f(mesh.normals[i1 * 3], mesh.normals[i1 * 3 + 1],
                 mesh.normals[i1 * 3 + 2]);
      glVertex3f(v[3], v[4], v[5]);
      glNormal3f(mesh.normals[i2 * 3], mesh.normals[i2 * 3 + 1],
                 mesh.normals[i2 * 3 + 2]);
      glVertex3f(v[6], v[7], v[8]);
    } else {
      const float ux = v[3] - v[0], uy = v[4] - v[1], uz = v[5] - v[2];
      const float vx = v[6] - v[0], vy = v[7] - v[1], vz = v[8] - v[2];
      float nx = uy * vz - uz * vy;
      float ny = uz * vx - ux * vz;
      float nz = ux * vy - uy * vx;
      const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
      if (len > 0.0f) {
        nx /= len;
        ny /= len;
        nz /= len;
      }
      glNormal3f(nx, ny, nz);
      glVertex3f(v[0], v[1], v[2]);
      glVertex3f(v[3], v[4], v[5]);
      glVertex3f(v[6], v[7], v[8]);
    }
  }
  glEnd();
}

wxBEGIN_EVENT_TABLE(FixturePreviewPanel, wxGLCanvas)
EVT_PAINT(FixturePreviewPanel::OnPaint)
EVT_SIZE(FixturePreviewPanel::OnResize)
EVT_LEFT_DOWN(FixturePreviewPanel::OnMouseDown)
EVT_LEFT_UP(FixturePreviewPanel::OnMouseUp)
EVT_MOTION(FixturePreviewPanel::OnMouseMove)
EVT_MOUSEWHEEL(FixturePreviewPanel::OnMouseWheel)
EVT_MOUSE_CAPTURE_LOST(FixturePreviewPanel::OnCaptureLost)
wxEND_EVENT_TABLE()

FixturePreviewPanel::FixturePreviewPanel(wxWindow *parent)
    : wxGLCanvas(parent, wxID_ANY, nullptr, wxDefaultPosition, wxSize(200, 200),
                 wxFULL_REPAINT_ON_RESIZE) {
  SetBackgroundStyle(wxBG_STYLE_CUSTOM);
  m_glContext = new wxGLContext(this);
  m_bbMin[0] = m_bbMin[1] = m_bbMin[2] = -0.1f;
  m_bbMax[0] = m_bbMax[1] = m_bbMax[2] = 0.1f;
  m_camera.SetOrientation(45.f, 30.f);
  m_camera.SetDistance(0.6f);
}

FixturePreviewPanel::~FixturePreviewPanel() { delete m_glContext; }

void FixturePreviewPanel::InitGL() {
  if (!IsShownOnScreen())
    return;
  SetCurrent(*m_glContext);
  if (!m_glInitialized) {
    glewExperimental = GL_TRUE;
    glewInit();
    m_glInitialized = true;
  }
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_COLOR_MATERIAL);
  glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
}

void FixturePreviewPanel::RebuildPosedNodes() {
  m_posedNodeWorld.assign(m_tree.nodes.size(), MatrixUtils::Identity());
  if (m_tree.nodes.empty()) {
    m_emitterDebugText = "Emitter debug: no emitter nodes";
    return;
  }

  int panAxisNode = -1;
  int tiltAxisNode = -1;
  for (size_t i = 0; i < m_tree.nodes.size(); ++i) {
    const auto &node = m_tree.nodes[i];
    if (node.type != GdtfNodeType::Axis)
      continue;
    wxString name = wxString::FromUTF8(node.stableName);
    wxString lower = name.Lower();
    if (panAxisNode < 0 && (lower.Contains("pan") || lower.Contains("yoke")))
      panAxisNode = static_cast<int>(i);
    if (tiltAxisNode < 0 && (lower.Contains("tilt") || lower.Contains("head")))
      tiltAxisNode = static_cast<int>(i);
  }
  if (panAxisNode < 0 && !m_tree.axisNodeIndices.empty())
    panAxisNode = m_tree.axisNodeIndices[0];
  if (tiltAxisNode < 0 && m_tree.axisNodeIndices.size() > 1)
    tiltAxisNode = m_tree.axisNodeIndices[1];

  const Matrix panRotation = MatrixUtils::EulerToMatrix(m_panDeg, 0.0f, 0.0f);
  const Matrix tiltRotation = MatrixUtils::EulerToMatrix(m_tiltDeg, 0.0f, 0.0f);

  for (size_t i = 0; i < m_tree.nodes.size(); ++i) {
    Matrix local = m_tree.nodes[i].localTransform;
    if (static_cast<int>(i) == panAxisNode)
      local = MatrixUtils::Multiply(local, panRotation);
    if (static_cast<int>(i) == tiltAxisNode)
      local = MatrixUtils::Multiply(local, tiltRotation);

    const int parent = m_tree.nodes[i].parentIndex;
    if (parent >= 0 && parent < static_cast<int>(m_posedNodeWorld.size()))
      m_posedNodeWorld[i] = MatrixUtils::Multiply(m_posedNodeWorld[parent], local);
    else
      m_posedNodeWorld[i] = local;
  }

  if (!m_tree.emitterNodeIndices.empty()) {
    const int emitterIndex = m_tree.emitterNodeIndices.front();
    if (emitterIndex >= 0 && emitterIndex < static_cast<int>(m_posedNodeWorld.size())) {
      const Matrix &w = m_posedNodeWorld[emitterIndex];
      std::array<float, 3> dir = w.w;
      const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
      if (len > 1e-6f) {
        dir[0] /= len;
        dir[1] /= len;
        dir[2] /= len;
      }
      m_emitterDebugText = wxString::Format("Emitter_%d dir=(%.3f, %.3f, %.3f)", emitterIndex,
                                            dir[0], dir[1], dir[2])
                               .ToStdString();
      return;
    }
  }
  m_emitterDebugText = "Emitter debug: no emitter nodes";
}

void FixturePreviewPanel::RecomputeBounds() {
  m_bbMin[0] = m_bbMin[1] = m_bbMin[2] = FLT_MAX;
  m_bbMax[0] = m_bbMax[1] = m_bbMax[2] = -FLT_MAX;

  if (!m_tree.nodes.empty()) {
    for (size_t i = 0; i < m_tree.nodes.size(); ++i) {
      const auto &node = m_tree.nodes[i];
      if (!node.hasMesh)
        continue;
      const Matrix &world = i < m_posedNodeWorld.size() ? m_posedNodeWorld[i] : node.worldTransform;
      for (size_t vi = 0; vi + 2 < node.mesh.vertices.size(); vi += 3) {
        std::array<float, 3> p = {node.mesh.vertices[vi] * RENDER_SCALE,
                                  node.mesh.vertices[vi + 1] * RENDER_SCALE,
                                  node.mesh.vertices[vi + 2] * RENDER_SCALE};
        p = TransformPoint(world, p);
        for (int j = 0; j < 3; ++j) {
          m_bbMin[j] = std::min(m_bbMin[j], p[j]);
          m_bbMax[j] = std::max(m_bbMax[j], p[j]);
        }
      }
    }
  } else if (m_hasModel) {
    for (const auto &obj : m_objects) {
      for (size_t vi = 0; vi + 2 < obj.mesh.vertices.size(); vi += 3) {
        std::array<float, 3> p = {obj.mesh.vertices[vi] * RENDER_SCALE,
                                  obj.mesh.vertices[vi + 1] * RENDER_SCALE,
                                  obj.mesh.vertices[vi + 2] * RENDER_SCALE};
        p = TransformPoint(obj.transform, p);
        for (int j = 0; j < 3; ++j) {
          m_bbMin[j] = std::min(m_bbMin[j], p[j]);
          m_bbMax[j] = std::max(m_bbMax[j], p[j]);
        }
      }
    }
  } else {
    m_bbMin[0] = m_bbMin[1] = m_bbMin[2] = -0.1f;
    m_bbMax[0] = m_bbMax[1] = m_bbMax[2] = 0.1f;
  }
}

void FixturePreviewPanel::LoadFixture(const std::string &gdtfPath) {
  m_objects.clear();
  m_tree = GdtfGeometryTree{};
  m_posedNodeWorld.clear();
  m_hasModel = false;
  if (!gdtfPath.empty()) {
    m_hasModel = LoadGdtf(gdtfPath, m_objects);
    LoadGdtfGeometryTree(gdtfPath, m_tree);
  }

  RebuildPosedNodes();
  RecomputeBounds();

  float cx = (m_bbMin[0] + m_bbMax[0]) * 0.5f;
  float cy = (m_bbMin[1] + m_bbMax[1]) * 0.5f;
  float cz = (m_bbMin[2] + m_bbMax[2]) * 0.5f;
  m_camera.SetTarget(cx, cy, cz);
  float sizeX = m_bbMax[0] - m_bbMin[0];
  float sizeY = m_bbMax[1] - m_bbMin[1];
  float sizeZ = m_bbMax[2] - m_bbMin[2];
  float radius = std::max({sizeX, sizeY, sizeZ}) * 0.5f;
  if (radius < 0.1f)
    radius = 0.1f;
  m_camera.SetDistance(radius * 3.0f);
  Refresh();
}

void FixturePreviewPanel::SetMotionPose(float panDeg, float tiltDeg, float dimmer) {
  m_panDeg = panDeg;
  m_tiltDeg = tiltDeg;
  m_dimmer = std::clamp(dimmer, 0.0f, 1.0f);
  RebuildPosedNodes();
  RecomputeBounds();
  Refresh();
}

std::string FixturePreviewPanel::GetEmitterDebugText() const { return m_emitterDebugText; }

void FixturePreviewPanel::Render() {
  int w, h;
  GetClientSize(&w, &h);
  glViewport(0, 0, w, h);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45.0, static_cast<double>(w) / h, 0.1, 100.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  m_camera.Apply();

  float lightPos[4] = {0.f, 0.f, 1.f, 0.f};
  float lightDiffuse[4] = {1.f, 1.f, 1.f, 1.f};
  float lightAmbient[4] = {0.2f, 0.2f, 0.2f, 1.f};
  glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
  glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);

  if (!m_tree.nodes.empty()) {
    for (size_t i = 0; i < m_tree.nodes.size(); ++i) {
      const auto &node = m_tree.nodes[i];
      if (!node.hasMesh)
        continue;

      glPushMatrix();
      const Matrix &world = i < m_posedNodeWorld.size() ? m_posedNodeWorld[i] : node.worldTransform;
      float m[16];
      MatrixToArray(world, m);
      glMultMatrixf(m);

      float r = 0.9f, g = 0.9f, b = 0.9f;
      if (node.isLens || node.type == GdtfNodeType::Emitter) {
        const float visualDimmer = 0.15f + 0.85f * m_dimmer;
        r = 1.0f * visualDimmer;
        g = 0.78f * visualDimmer;
        b = 0.35f * visualDimmer;
      }
      glColor3f(r, g, b);
      DrawMesh(node.mesh, RENDER_SCALE);
      glPopMatrix();
    }

    if (!m_tree.emitterNodeIndices.empty()) {
      const int emitterIndex = m_tree.emitterNodeIndices.front();
      if (emitterIndex >= 0 && emitterIndex < static_cast<int>(m_posedNodeWorld.size())) {
        const Matrix &emitter = m_posedNodeWorld[emitterIndex];
        const float scale = 0.2f;
        std::array<float, 3> origin = {emitter.o[0], emitter.o[1], emitter.o[2]};
        std::array<float, 3> tip = {origin[0] + emitter.w[0] * scale,
                                    origin[1] + emitter.w[1] * scale,
                                    origin[2] + emitter.w[2] * scale};
        glDisable(GL_LIGHTING);
        glLineWidth(2.0f);
        glColor3f(1.0f, 0.2f, 0.2f);
        glBegin(GL_LINES);
        glVertex3f(origin[0], origin[1], origin[2]);
        glVertex3f(tip[0], tip[1], tip[2]);
        glEnd();
        glEnable(GL_LIGHTING);
      }
    }
  } else if (m_hasModel) {
    glColor3f(1.0f, 1.0f, 1.0f);
    for (const auto &obj : m_objects) {
      glPushMatrix();
      float m[16];
      MatrixToArray(obj.transform, m);
      glMultMatrixf(m);
      DrawMesh(obj.mesh, RENDER_SCALE);
      glPopMatrix();
    }
  } else {
    glColor3f(1.0f, 1.0f, 1.0f);
    DrawCube(0.2f);
  }

  glFlush();
}

void FixturePreviewPanel::OnPaint(wxPaintEvent &) {
  wxPaintDC dc(this);
  if (!IsShownOnScreen())
    return;
  InitGL();
  Render();
  SwapBuffers();
}

void FixturePreviewPanel::OnResize(wxSizeEvent &) { Refresh(); }

void FixturePreviewPanel::OnMouseDown(wxMouseEvent &evt) {
  m_dragging = true;
  m_lastMousePos = evt.GetPosition();
  CaptureMouse();
}

void FixturePreviewPanel::OnMouseUp(wxMouseEvent &) {
  if (m_dragging) {
    m_dragging = false;
    if (HasCapture())
      ReleaseMouse();
  }
}

void FixturePreviewPanel::OnCaptureLost(wxMouseCaptureLostEvent &WXUNUSED(evt)) {
  m_dragging = false;
}

void FixturePreviewPanel::OnMouseMove(wxMouseEvent &evt) {
  if (m_dragging) {
    wxPoint pos = evt.GetPosition();
    int dx = pos.x - m_lastMousePos.x;
    int dy = pos.y - m_lastMousePos.y;
    m_camera.Orbit(dx * 0.5f, dy * 0.5f);
    m_lastMousePos = pos;
    Refresh();
  }
}

void FixturePreviewPanel::OnMouseWheel(wxMouseEvent &evt) {
  int rot = evt.GetWheelRotation();
  int delta = evt.GetWheelDelta();
  m_camera.Zoom(-static_cast<float>(rot) / delta);
  Refresh();
}
