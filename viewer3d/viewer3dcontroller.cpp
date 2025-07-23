/*
 * File: viewer3dcontroller.cpp
 * Author: Luisma Peramato
 * License: MIT
 * Description: Implementation of 3D viewer logic.
 */

#include "viewer3dcontroller.h"
#include "scenedatamanager.h"
#include "configmanager.h"
#include "loader3ds.h"
#include "types.h"
#include "consolepanel.h"
#include <wx/wx.h>
#include <filesystem>
#include <cfloat>
#include <array>

namespace fs = std::filesystem;


#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>
#include <cmath>

static std::string FindFileRecursive(const std::string& baseDir,
                                     const std::string& fileName)
{
    if (baseDir.empty())
        return {};
    for (auto& p : fs::recursive_directory_iterator(baseDir)) {
        if (!p.is_regular_file())
            continue;
        if (p.path().filename() == fileName)
            return p.path().string();
    }
    return {};
}

static std::string ResolveGdtfPath(const std::string& base,
                                   const std::string& spec)
{
    std::string p = base.empty() ? spec : (fs::path(base) / spec).string();
    if (fs::exists(p))
        return p;
    return FindFileRecursive(base, fs::path(spec).filename().string());
}

static void MatrixToArray(const Matrix& m, float out[16])
{
    out[0] = m.u[0];  out[1] = m.u[1];  out[2] = m.u[2];  out[3] = 0.0f;
    out[4] = m.v[0];  out[5] = m.v[1];  out[6] = m.v[2];  out[7] = 0.0f;
    out[8] = m.w[0];  out[9] = m.w[1];  out[10] = m.w[2]; out[11] = 0.0f;
    out[12] = m.o[0]; out[13] = m.o[1]; out[14] = m.o[2]; out[15] = 1.0f;
}

struct ScreenRect {
    double minX = DBL_MAX;
    double minY = DBL_MAX;
    double maxX = -DBL_MAX;
    double maxY = -DBL_MAX;
};

static std::array<float,3> TransformPoint(const Matrix& m, const std::array<float,3>& p)
{
    return {
        m.u[0]*p[0] + m.v[0]*p[1] + m.w[0]*p[2] + m.o[0],
        m.u[1]*p[0] + m.v[1]*p[1] + m.w[1]*p[2] + m.o[1],
        m.u[2]*p[0] + m.v[2]*p[1] + m.w[2]*p[2] + m.o[2]
    };
}

Viewer3DController::Viewer3DController() {}

Viewer3DController::~Viewer3DController() {}

void Viewer3DController::SetHighlightUuid(const std::string& uuid) {
    m_highlightUuid = uuid;
}

void Viewer3DController::SetSelectedUuids(const std::vector<std::string>& uuids) {
    m_selectedUuids.clear();
    for (const auto& u : uuids)
        m_selectedUuids.insert(u);
}

// Loads meshes or GDTF models referenced by scene objects. Called when the scene is updated.
void Viewer3DController::Update() {
    const std::string& base = ConfigManager::Get().GetScene().basePath;

    const auto& trusses = SceneDataManager::Instance().GetTrusses();
    for (const auto& [uuid, t] : trusses) {
        if (t.symbolFile.empty())
            continue;

        std::string path = base.empty() ? t.symbolFile
                                         : (fs::path(base) / t.symbolFile).string();
        if (m_loadedMeshes.find(path) == m_loadedMeshes.end()) {
            Mesh mesh;
            if (Load3DS(path, mesh)) {
                m_loadedMeshes[path] = std::move(mesh);
            } else if (ConsolePanel::Instance()) {
                wxString msg = wxString::Format("Failed to load 3DS: %s", wxString::FromUTF8(path));
                ConsolePanel::Instance()->AppendMessage(msg);
            }
        }
    }

    const auto& objects = SceneDataManager::Instance().GetSceneObjects();
    for (const auto& [uuid, obj] : objects) {
        if (obj.modelFile.empty())
            continue;
        std::string path = base.empty() ? obj.modelFile
                                         : (fs::path(base) / obj.modelFile).string();
        if (m_loadedMeshes.find(path) == m_loadedMeshes.end()) {
            Mesh mesh;
            if (Load3DS(path, mesh)) {
                m_loadedMeshes[path] = std::move(mesh);
            } else if (ConsolePanel::Instance()) {
                wxString msg = wxString::Format("Failed to load 3DS: %s", wxString::FromUTF8(path));
                ConsolePanel::Instance()->AppendMessage(msg);
            }
        }
    }

    const auto& fixtures = SceneDataManager::Instance().GetFixtures();
    for (const auto& [uuid, f] : fixtures) {
        if (f.gdtfSpec.empty())
            continue;
        std::string gdtfPath = ResolveGdtfPath(base, f.gdtfSpec);
        if (gdtfPath.empty()) {
            if (ConsolePanel::Instance()) {
                wxString msg = wxString::Format("GDTF file not found: %s", wxString::FromUTF8(f.gdtfSpec));
                ConsolePanel::Instance()->AppendMessage(msg);
            }
            continue;
        }
        if (m_loadedGdtf.find(gdtfPath) == m_loadedGdtf.end()) {
            std::vector<GdtfObject> objs;
            if (LoadGdtf(gdtfPath, objs)) {
                m_loadedGdtf[gdtfPath] = std::move(objs);
            } else if (ConsolePanel::Instance()) {
                wxString msg = wxString::Format("Failed to load GDTF: %s", wxString::FromUTF8(gdtfPath));
                ConsolePanel::Instance()->AppendMessage(msg);
            }
        }
    }

    // Precompute bounding boxes for hover detection
    m_fixtureBounds.clear();
    for (const auto& [uuid, f] : fixtures) {
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
            for (const auto& obj : itg->second) {
                for (size_t vi = 0; vi + 2 < obj.mesh.vertices.size(); vi += 3) {
                    std::array<float,3> p = {
                        obj.mesh.vertices[vi] * RENDER_SCALE,
                        obj.mesh.vertices[vi + 1] * RENDER_SCALE,
                        obj.mesh.vertices[vi + 2] * RENDER_SCALE
                    };
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
            std::array<std::array<float,3>,8> corners = {
                std::array<float,3>{-half,-half,-half}, {half,-half,-half},
                {-half,half,-half}, {half,half,-half},
                {-half,-half,half}, {half,-half,half},
                {-half,half,half}, {half,half,half}
            };
            for (const auto& c : corners) {
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
}

// Renders all scene objects using their transformMatrix
void Viewer3DController::RenderScene()
{
    SetupBasicLighting();
    DrawGrid();

    // Fixtures
    const auto& fixtures = SceneDataManager::Instance().GetFixtures();
    const std::string& base = ConfigManager::Get().GetScene().basePath;
    for (const auto& [uuid, f] : fixtures) {
        glPushMatrix();

        bool highlight = (!m_highlightUuid.empty() && uuid == m_highlightUuid);
        bool selected = (m_selectedUuids.find(uuid) != m_selectedUuids.end());

        float matrix[16];
        MatrixToArray(f.transform, matrix);
        ApplyTransform(matrix, true);


        std::string gdtfPath = ResolveGdtfPath(base, f.gdtfSpec);
        auto itg = m_loadedGdtf.find(gdtfPath);

        if (itg != m_loadedGdtf.end()) {
            for (const auto& obj : itg->second) {
                glPushMatrix();
                float m2[16];
                MatrixToArray(obj.transform, m2);
                // GDTF geometry offsets are defined relative to the fixture
                // in meters. Only the vertex coordinates need unit scaling.
                ApplyTransform(m2, false);
                DrawMeshWithOutline(obj.mesh, 1.0f, 1.0f, 1.0f, RENDER_SCALE, highlight, selected);
                glPopMatrix();
            }
        } else {
            DrawCubeWithOutline(0.2f, 0.8f, 0.8f, 1.0f, highlight, selected);
        }

        glPopMatrix();
    }

    // Trusses
    const auto& trusses = SceneDataManager::Instance().GetTrusses();
    for (const auto& [uuid, t] : trusses) {
        glPushMatrix();

        float matrix[16];
        MatrixToArray(t.transform, matrix);
        ApplyTransform(matrix, true);




        if (!t.symbolFile.empty()) {
            std::string path = base.empty() ? t.symbolFile
                                             : (fs::path(base) / t.symbolFile).string();
            auto it = m_loadedMeshes.find(path);
            if (it != m_loadedMeshes.end()) {
                DrawMeshWithOutline(it->second);
            } else {
                DrawCubeWithOutline(0.3f, 0.5f, 0.5f);
            }
        } else {
            DrawCubeWithOutline(0.3f, 0.5f, 0.5f);
        }

        glPopMatrix();
    }

    const auto& meshes = SceneDataManager::Instance().GetSceneObjects();
    for (const auto& [uuid, m] : meshes) {
        glPushMatrix();

        float matrix[16];
        MatrixToArray(m.transform, matrix);
        ApplyTransform(matrix, true);



        if (!m.modelFile.empty()) {
            std::string path = base.empty() ? m.modelFile
                                            : (fs::path(base) / m.modelFile).string();
            auto it = m_loadedMeshes.find(path);
            if (it != m_loadedMeshes.end())
                DrawMeshWithOutline(it->second);
            else
                DrawCubeWithOutline(0.3f, 0.8f, 0.8f, 0.8f);
        } else {
            DrawCubeWithOutline(0.3f, 0.8f, 0.8f, 0.8f);
        }

        glPopMatrix();
    }

    const auto& groups = SceneDataManager::Instance().GetGroupObjects();
    for (const auto& [uuid, g] : groups) {
        (void)uuid; (void)g; // groups not implemented
    }

    DrawAxes();
}


// Draws a solid cube centered at origin with given size and color
void Viewer3DController::DrawCube(float size, float r, float g, float b)
{
    float half = size / 2.0f;
    float x0 = -half, x1 = half;
    float y0 = -half, y1 = half;
    float z0 = -half, z1 = half;

    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1); // Front
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(x1, y0, z0); glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0); // Back
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0); // Left
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(x1, y0, z1); glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1); // Right
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0); // Top
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(x0, y0, z1); // Bottom
    glEnd();
}

// Draws a wireframe cube centered at origin with given size
void Viewer3DController::DrawWireframeCube(float size)
{
    float half = size / 2.0f;
    float x0 = -half, x1 = half;
    float y0 = -half, y1 = half;
    float z0 = -half, z1 = half;

    glColor3f(1.0f, 1.0f, 0.0f);
    glBegin(GL_LINES);
    glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0);
    glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0);
    glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1);
    glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0);
    glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1);
    glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1);
    glVertex3f(x0, y1, z0); glVertex3f(x0, y1, z1);
    glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1);
    glEnd();
}

// Draws a colored cube with a black outline
void Viewer3DController::DrawCubeWithOutline(float size, float r, float g, float b, bool highlight, bool selected)
{
    // Render a slightly expanded cube in black using front face culling to
    // mimic a silhouette outline.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glPushMatrix();
    float outlineScale = 1.03f;
    if (selected)
        outlineScale = 1.15f;
    else if (highlight)
        outlineScale = 1.1f;
    glScalef(outlineScale, outlineScale, outlineScale);
    if (selected)
        DrawCube(size, 0.0f, 1.0f, 1.0f);
    else if (highlight)
        DrawCube(size, 0.0f, 1.0f, 0.0f);
    else
        DrawCube(size, 0.0f, 0.0f, 0.0f);
    glPopMatrix();
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);

    // Actual colored cube
    DrawCube(size, r, g, b);
}

// Draws a mesh with a black outline using the given color
void Viewer3DController::DrawMeshWithOutline(const Mesh& mesh, float r, float g,
                                             float b, float scale, bool highlight,
                                             bool selected)
{
    // Draw the mesh slightly scaled up in black with front face culling to
    // create a silhouette outline. This avoids drawing all internal triangle
    // edges in black like a wireframe.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glPushMatrix();
    float outlineScale = 1.03f; // expansion for outline
    if (selected)
        outlineScale = 1.15f;
    else if (highlight)
        outlineScale = 1.1f;
    glScalef(outlineScale, outlineScale, outlineScale);
    if (selected)
        glColor3f(0.0f, 1.0f, 1.0f);
    else if (highlight)
        glColor3f(0.0f, 1.0f, 0.0f);
    else
        glColor3f(0.0f, 0.0f, 0.0f);
    DrawMesh(mesh, scale);
    glPopMatrix();
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);

    // Draw the actual mesh on top
    glColor3f(r, g, b);
    DrawMesh(mesh, scale);
}

// Draws a mesh using GL triangles. The optional scale parameter allows
// converting vertex units (e.g. millimeters) to meters.
void Viewer3DController::DrawMesh(const Mesh& mesh, float scale)
{
    glBegin(GL_TRIANGLES);
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
            nx /= len; ny /= len; nz /= len;
        }

        glNormal3f(nx, ny, nz);
        glVertex3f(v0x, v0y, v0z);
        glVertex3f(v1x, v1y, v1z);
        glVertex3f(v2x, v2y, v2z);
    }
    glEnd();
}

// Draws the ground grid on the Z=0 plane
void Viewer3DController::DrawGrid()
{
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
void Viewer3DController::DrawAxes()
{
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor3f(1.0f, 0.0f, 0.0f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(1.0f, 0.0f, 0.0f); // X
    glColor3f(0.0f, 1.0f, 0.0f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 1.0f, 0.0f); // Y
    glColor3f(0.0f, 0.0f, 1.0f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 0.0f, 1.0f); // Z
    glEnd();
}

// Multiplies the current matrix by the given transform. When
// scaleTranslation is true the translation part is converted from
// millimeters to meters using RENDER_SCALE.
void Viewer3DController::ApplyTransform(const float matrix[16], bool scaleTranslation)
{
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

    GLfloat ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    GLfloat specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat position[] = { 2.0f, -4.0f, 5.0f, 0.0f };

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

void Viewer3DController::DrawFixtureLabels(wxDC& dc, int width, int height)
{
    double model[16];
    double proj[16];
    int viewport[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glGetIntegerv(GL_VIEWPORT, viewport);

    const auto& fixtures = SceneDataManager::Instance().GetFixtures();
    for (const auto& [uuid, f] : fixtures) {
        if (uuid != m_highlightUuid && m_selectedUuids.find(uuid) == m_selectedUuids.end())
            continue;

        double sx, sy, sz;
        double wx = f.transform.o[0] * RENDER_SCALE;
        double wy = f.transform.o[1] * RENDER_SCALE;
        double wz = f.transform.o[2] * RENDER_SCALE;
        if (gluProject(wx, wy, wz, model, proj, viewport, &sx, &sy, &sz) == GL_TRUE) {
            int x = static_cast<int>(sx);
            int y = height - static_cast<int>(sy);
            wxString label = f.name.empty() ? wxString::FromUTF8(uuid)
                                           : wxString::FromUTF8(f.name);
            label += "\nID: " + wxString::Format("%d", f.fixtureId);
            if (!f.address.empty())
                label += "\n" + wxString::FromUTF8(f.address);
            dc.DrawText(label, x, y);
        }
    }
}

bool Viewer3DController::GetFixtureLabelAt(int mouseX, int mouseY,
                                           int width, int height,
                                           wxString& outLabel, wxPoint& outPos,
                                           std::string* outUuid)
{
    double model[16];
    double proj[16];
    int viewport[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glGetIntegerv(GL_VIEWPORT, viewport);

    const auto& fixtures = SceneDataManager::Instance().GetFixtures();

    for (const auto& [uuid, f] : fixtures) {
        auto bit = m_fixtureBounds.find(uuid);
        if (bit == m_fixtureBounds.end())
            continue;

        const BoundingBox& bb = bit->second;
        std::array<std::array<float,3>,8> corners = {
            std::array<float,3>{bb.min[0], bb.min[1], bb.min[2]},
            {bb.max[0], bb.min[1], bb.min[2]},
            {bb.min[0], bb.max[1], bb.min[2]},
            {bb.max[0], bb.max[1], bb.min[2]},
            {bb.min[0], bb.min[1], bb.max[2]},
            {bb.max[0], bb.min[1], bb.max[2]},
            {bb.min[0], bb.max[1], bb.max[2]},
            {bb.max[0], bb.max[1], bb.max[2]}
        };

        ScreenRect rect;
        for (const auto& c : corners) {
            double sx, sy, sz;
            if (gluProject(c[0], c[1], c[2], model, proj, viewport, &sx, &sy, &sz) == GL_TRUE) {
                rect.minX = std::min(rect.minX, sx);
                rect.maxX = std::max(rect.maxX, sx);
                double sy2 = height - sy;
                rect.minY = std::min(rect.minY, sy2);
                rect.maxY = std::max(rect.maxY, sy2);
            }
        }

        if (mouseX >= rect.minX && mouseX <= rect.maxX &&
            mouseY >= rect.minY && mouseY <= rect.maxY) {
            outPos.x = static_cast<int>((rect.minX + rect.maxX) * 0.5);
            outPos.y = static_cast<int>((rect.minY + rect.maxY) * 0.5);
            wxString label = f.name.empty() ? wxString::FromUTF8(uuid)
                                           : wxString::FromUTF8(f.name);
            label += "\nID: " + wxString::Format("%d", f.fixtureId);
            if (!f.address.empty())
                label += "\n" + wxString::FromUTF8(f.address);
            outLabel = label;
            if (outUuid)
                *outUuid = uuid;
            return true;
        }
    }
    return false;
}

