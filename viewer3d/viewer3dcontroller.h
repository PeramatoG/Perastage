/*
 * File: viewer3dcontroller.h
 * Author: Luisma Peramato
 * License: MIT
 * Description: Controller class for 3D viewer logic and state.
 */

#pragma once

#include "scenedatamanager.h"
#include "mesh.h"
#include "gdtfloader.h"
#include <unordered_map>
#include <array>
#include <wx/dcclient.h>
#include <wx/gdicmn.h>
#include <wx/string.h>

// MVR coordinates are defined in millimeters. This constant converts
// them to meters when rendering.
static constexpr float RENDER_SCALE = 0.001f;


class Viewer3DController
{
public:
    Viewer3DController();
    ~Viewer3DController();

    // Updates the internal logic if needed (e.g. animation, camera)
    void Update();

    // Renders all scene objects
    void RenderScene();

    // Draws fixture names at their world positions using a 2D device context
    void DrawFixtureLabels(wxDC& dc, int width, int height);

    // Returns true and outputs label and screen position of the fixture
    // under the given mouse coordinates (if any)
    bool GetFixtureLabelAt(int mouseX, int mouseY,
                           int width, int height,
                           wxString& outLabel, wxPoint& outPos);

private:
    // Draws a solid cube centered at origin with given size and color
    void DrawCube(float size = 0.2f, float r = 1.0f, float g = 1.0f, float b = 1.0f);

    // Draws a wireframe cube centered at origin with given size
    void DrawWireframeCube(float size = 0.3f);

    // Draws a colored mesh with a black outline for a sketch effect
    void DrawMeshWithOutline(const Mesh& mesh, float r = 1.0f, float g = 1.0f,
                             float b = 1.0f, float scale = RENDER_SCALE);

    // Draws a colored cube with a black outline
    void DrawCubeWithOutline(float size = 0.2f, float r = 1.0f, float g = 1.0f,
                             float b = 1.0f);

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
    void DrawMesh(const Mesh& mesh, float scale = RENDER_SCALE);

    // Cache of already loaded meshes indexed by absolute file path
    std::unordered_map<std::string, Mesh> m_loadedMeshes;

    // Cache of loaded GDTF models indexed by absolute file path
    std::unordered_map<std::string, std::vector<GdtfObject>> m_loadedGdtf;

    struct BoundingBox {
        std::array<float,3> min;
        std::array<float,3> max;
    };

    // Precomputed world-space bounds for each fixture by UUID
    std::unordered_map<std::string, BoundingBox> m_fixtureBounds;
};
