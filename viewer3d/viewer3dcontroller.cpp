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

namespace fs = std::filesystem;


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>

constexpr float RENDER_SCALE = 0.001f;

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

Viewer3DController::Viewer3DController() {}

Viewer3DController::~Viewer3DController() {}

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
                ConsolePanel::Instance()->AppendMessage("Failed to load 3DS: " + wxString::FromUTF8(path));
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
                ConsolePanel::Instance()->AppendMessage("Failed to load 3DS: " + wxString::FromUTF8(path));
            }
        }
    }

    const auto& fixtures = SceneDataManager::Instance().GetFixtures();
    for (const auto& [uuid, f] : fixtures) {
        if (f.gdtfSpec.empty())
            continue;
        std::string gdtfPath = ResolveGdtfPath(base, f.gdtfSpec);
        if (gdtfPath.empty()) {
            if (ConsolePanel::Instance())
                ConsolePanel::Instance()->AppendMessage("GDTF file not found: " + wxString::FromUTF8(f.gdtfSpec));
            continue;
        }
        if (m_loadedGdtf.find(gdtfPath) == m_loadedGdtf.end()) {
            std::vector<GdtfObject> objs;
            if (LoadGdtf(gdtfPath, objs)) {
                m_loadedGdtf[gdtfPath] = std::move(objs);
            } else if (ConsolePanel::Instance()) {
                ConsolePanel::Instance()->AppendMessage("Failed to load GDTF: " + wxString::FromUTF8(gdtfPath));
            }
        }
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

        float matrix[16];
        MatrixToArray(f.transform, matrix);
        ApplyScaledTransform(matrix);

        std::string gdtfPath = ResolveGdtfPath(base, f.gdtfSpec);
        auto itg = m_loadedGdtf.find(gdtfPath);

        if (itg != m_loadedGdtf.end()) {
            for (const auto& obj : itg->second) {
                glPushMatrix();
                float m2[16];
                MatrixToArray(obj.transform, m2);
                ApplyScaledTransform(m2);
                DrawMesh(obj.mesh);
                glPopMatrix();
            }
        } else {
            DrawCube(0.2f, 0.8f, 0.8f);
        }

        glPopMatrix();
    }

    // Trusses
    const auto& trusses = SceneDataManager::Instance().GetTrusses();
    for (const auto& [uuid, t] : trusses) {
        glPushMatrix();

        float matrix[16];
        MatrixToArray(t.transform, matrix);
        matrix[12] *= RENDER_SCALE;
        matrix[13] *= RENDER_SCALE;
        matrix[14] *= RENDER_SCALE;

        glMultMatrixf(matrix);

        if (!t.symbolFile.empty()) {
            std::string path = base.empty() ? t.symbolFile
                                             : (fs::path(base) / t.symbolFile).string();
            auto it = m_loadedMeshes.find(path);
            if (it != m_loadedMeshes.end()) {
                DrawMesh(it->second);
            } else {
                DrawCube(0.3f, 0.5f, 0.5f);
            }
        } else {
            DrawCube(0.3f, 0.5f, 0.5f);
        }

        glPopMatrix();
    }

    const auto& meshes = SceneDataManager::Instance().GetSceneObjects();
    for (const auto& [uuid, m] : meshes) {
        glPushMatrix();

        float matrix[16];
        MatrixToArray(m.transform, matrix);
        ApplyScaledTransform(matrix);

        if (!m.modelFile.empty()) {
            std::string path = base.empty() ? m.modelFile
                                            : (fs::path(base) / m.modelFile).string();
            auto it = m_loadedMeshes.find(path);
            if (it != m_loadedMeshes.end())
                DrawMesh(it->second);
            else
                DrawWireframeCube(0.3f);
        } else {
            DrawWireframeCube(0.3f);
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
    glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1); // Front
    glVertex3f(x1, y0, z0); glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0); // Back
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0); // Left
    glVertex3f(x1, y0, z1); glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1); // Right
    glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0); // Top
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

// Draws a mesh using GL triangles. Vertices are assumed to be in millimeters.
void Viewer3DController::DrawMesh(const Mesh& mesh)
{
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        for (int v = 0; v < 3; ++v) {
            unsigned short idx = mesh.indices[i + v];
            float x = mesh.vertices[idx * 3] * RENDER_SCALE;
            float y = mesh.vertices[idx * 3 + 1] * RENDER_SCALE;
            float z = mesh.vertices[idx * 3 + 2] * RENDER_SCALE;
            glVertex3f(x, y, z);
        }
    }
    glEnd();
}

// Draws the ground grid on the Z=0 plane
void Viewer3DController::DrawGrid()
{
    const float size = 20.0f;
    const float step = 1.0f;

    glColor3f(0.3f, 0.3f, 0.3f);
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

// Applies uniform scaling and then the object's transformation matrix
void Viewer3DController::ApplyScaledTransform(const float matrix[16])
{
    float scaledMatrix[16];
    std::copy(matrix, matrix + 16, scaledMatrix);
    scaledMatrix[12] *= RENDER_SCALE;
    scaledMatrix[13] *= RENDER_SCALE;
    scaledMatrix[14] *= RENDER_SCALE;
    glMultMatrixf(scaledMatrix);
}


// Initializes simple lighting for the scene
void Viewer3DController::SetupBasicLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    GLfloat specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat position[] = { 2.0f, 4.0f, 5.0f, 0.0f };

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

