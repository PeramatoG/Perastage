/*
 * File: viewer3dcontroller.h
 * Author: Luisma Peramato
 * License: MIT
 * Description: Controller class for 3D viewer logic and state.
 */

#pragma once

#include "gdtfloader.h"
#include "mesh.h"
#include "scenedatamanager.h"
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <wx/gdicmn.h>
#include <wx/string.h>

struct NVGcontext;

// MVR coordinates are defined in millimeters. This constant converts
// them to meters when rendering.
static constexpr float RENDER_SCALE = 0.001f;

class Viewer3DController {
public:
  Viewer3DController();
  ~Viewer3DController();

  // Initializes OpenGL-dependent resources (NanoVG context, fonts)
  void InitializeGL();

  // Updates the internal logic if needed (e.g. animation, camera)
  void Update();

  // Renders all scene objects
  void RenderScene();

  // Fixture UUID currently highlighted (hovered)
  void SetHighlightUuid(const std::string &uuid);
  void SetSelectedUuids(const std::vector<std::string> &uuids);

  // Draws object names at their projected positions using OpenGL
  void DrawFixtureLabels(int width, int height);
  void DrawTrussLabels(int width, int height);
  void DrawSceneObjectLabels(int width, int height);

  // Returns true and outputs label and screen position of the fixture
  // under the given mouse coordinates (if any)
  bool GetFixtureLabelAt(int mouseX, int mouseY, int width, int height,
                         wxString &outLabel, wxPoint &outPos,
                         std::string *outUuid = nullptr);
  bool GetTrussLabelAt(int mouseX, int mouseY, int width, int height,
                       wxString &outLabel, wxPoint &outPos,
                       std::string *outUuid = nullptr);
  bool GetSceneObjectLabelAt(int mouseX, int mouseY, int width, int height,
                             wxString &outLabel, wxPoint &outPos,
                             std::string *outUuid = nullptr);

private:
  // Draws a solid cube centered at origin with given size and color
  void DrawCube(float size = 0.2f, float r = 1.0f, float g = 1.0f,
                float b = 1.0f);

  // Draws a wireframe cube centered at origin with given size
  void DrawWireframeCube(float size = 0.3f);

  // Draws a wireframe box with independent dimensions. Length corresponds
  // to the X axis, width to Y and height to Z. The box's origin is at the
  // left end (X = 0), centered in Y and resting on Z = 0 so its base aligns
  // with the ground. It is tinted based on selection/highlight state.
  void DrawWireframeBox(float length, float height, float width,
                        bool highlight = false, bool selected = false);

  // Draws a colored mesh. When selected or highlighted the mesh is tinted
  // in cyan or green. The optional center offset parameters are kept for
  // backwards compatibility but currently unused.
  void DrawMeshWithOutline(const Mesh &mesh, float r = 1.0f, float g = 1.0f,
                           float b = 1.0f, float scale = RENDER_SCALE,
                           bool highlight = false, bool selected = false,
                           float cx = 0.0f, float cy = 0.0f, float cz = 0.0f);

  // Draws a colored cube tinted when selected or highlighted. The optional
  // center offset parameters are unused and kept for compatibility.
  void DrawCubeWithOutline(float size = 0.2f, float r = 1.0f, float g = 1.0f,
                           float b = 1.0f, bool highlight = false,
                           bool selected = false, float cx = 0.0f,
                           float cy = 0.0f, float cz = 0.0f);

  // Applies the object's transformation matrix. When scaleTranslation
  // is true the translation part is converted from millimeters to
  // meters using the render scale factor.
  void ApplyTransform(const float matrix[16], bool scaleTranslation = true);

  // Draws the reference grid on the Z=0 plane
  void DrawGrid();

  // Draws the XYZ axis lines
  void DrawAxes();

  // Initializes simple lighting for the scene
  void SetupBasicLighting();
  void SetupMaterialFromRGB(float r, float g, float b);

  // Draws a mesh loaded from a 3DS file using the given scale factor
  // for vertex positions. GDTF models may already be defined in meters
  // so they can use a scale of 1.0f
  void DrawMesh(const Mesh &mesh, float scale = RENDER_SCALE);

  // Cache of already loaded meshes indexed by absolute file path
  std::unordered_map<std::string, Mesh> m_loadedMeshes;

  // Cache of loaded GDTF models indexed by absolute file path
  std::unordered_map<std::string, std::vector<GdtfObject>> m_loadedGdtf;

  struct BoundingBox {
    std::array<float, 3> min;
    std::array<float, 3> max;
  };

  // Precomputed world-space bounds for each fixture by UUID
  std::unordered_map<std::string, BoundingBox> m_fixtureBounds;
  std::unordered_map<std::string, BoundingBox> m_trussBounds;
  std::unordered_map<std::string, BoundingBox> m_objectBounds;

  // Currently highlighted fixture UUID
  std::string m_highlightUuid;
  // Currently selected fixture UUIDs
  std::unordered_set<std::string> m_selectedUuids;

  // NanoVG context used to render on-screen labels
  NVGcontext *m_vg = nullptr;
  // Font handle for label rendering
  int m_font = -1;
};
