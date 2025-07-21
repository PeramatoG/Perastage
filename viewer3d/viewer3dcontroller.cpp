/*
 * File: viewer3dcontroller.cpp
 * Author: Luisma Peramato
 * License: MIT
 * Description: Implementation of 3D viewer logic.
 */

#include "viewer3dcontroller.h"
#include "scenedatamanager.h"
#include "threemodelassimp.h"
#include "compositemodel.h"


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <assimp/matrix4x4.h>
#include <iostream>
#include <wx/math.h>

constexpr float RENDER_SCALE = 0.001f;

Viewer3DController::Viewer3DController() {}

Viewer3DController::~Viewer3DController() {}

void Viewer3DController::Update() {}

// Renders all scene objects using their transformMatrix
void Viewer3DController::RenderScene()
{
    SetupBasicLighting();
    DrawGrid();

    // Fixtures
    const auto& fixtures = SceneDataManager::Instance().GetFixtures();
    for (const auto& [uuid, f] : fixtures) {
        glPushMatrix();

        // Apply scaled transformation matrix from GeneralSceneDescription
        float matrix[16];
        std::copy(std::begin(f.transformMatrix), std::end(f.transformMatrix), matrix);
        matrix[12] *= RENDER_SCALE;
        matrix[13] *= RENDER_SCALE;
        matrix[14] *= RENDER_SCALE;

        glMultMatrixf(matrix);

        if (f.model) {
            f.model->Render();
        }

        else {
            DrawCube(0.2f, 0.8f, 0.8f); // Placeholder if model is missing
        }


        glPopMatrix();
    }

    // Trusses
    const auto& trusses = SceneDataManager::Instance().GetTrusses();
    for (const auto& [uuid, t] : trusses) {
        glPushMatrix();

        // Apply scaled transformation matrix
        float matrix[16];
        std::copy(std::begin(t.transformMatrix), std::end(t.transformMatrix), matrix);
        matrix[12] *= RENDER_SCALE;
        matrix[13] *= RENDER_SCALE;
        matrix[14] *= RENDER_SCALE;

        glMultMatrixf(matrix);

        if (t.model) {
            // std::cout << "[Render] Rendering truss model: " << t.name << " UUID: " << t.uuid << std::endl;
            ApplyRotationCorrectionIfNeeded(t.model, matrix);

            /*

            // Convert quaternion to Euler angles (degrees)
            aiVector3D euler = QuaternionToEulerDegrees(rotation);

            // Convert position from meters to millimeters for display
            int x_mm = static_cast<int>(matrix[12] * 1000.0f);
            int y_mm = static_cast<int>(matrix[13] * 1000.0f);
            int z_mm = static_cast<int>(matrix[14] * 1000.0f);

            std::cout << "[Truss Debug] UUID: " << t.uuid << "\n";
            std::cout << " - Model: " << t.name << "\n";
            std::cout << " - Position (mm): x=" << x_mm << ", y=" << y_mm << ", z=" << z_mm << "\n";
            std::cout << " - Rotation (deg): x=" << euler.x << ", y=" << euler.y << ", z=" << euler.z << "\n";

            std::cout << "[Matrix Debug] Matrix for UUID: " << t.uuid << " (scaled):\n";
            for (int i = 0; i < 4; ++i) {
                std::cout << "  ";
                for (int j = 0; j < 4; ++j) {
                    std::cout << matrix[j * 4 + i] << " ";
                }
                std::cout << "\n";
            }

            */

            SetupMaterialFromColor(t.color);
            t.model->render(); // Renders using local offset
        }
        else {
            SetupMaterialFromColor(t.color);
            DrawCube(t.length * RENDER_SCALE, 0.5f, 0.5f); // Dummy truss
        }

        glPopMatrix();
    }

    const auto& meshes = SceneDataManager::Instance().GetSceneObjects();
    for (const auto& [uuid, m] : meshes) {
        glPushMatrix();

        // Apply scaled transformation matrix
        float matrix[16];
        std::copy(std::begin(m.transformMatrix), std::end(m.transformMatrix), matrix);
        matrix[12] *= RENDER_SCALE;
        matrix[13] *= RENDER_SCALE;
        matrix[14] *= RENDER_SCALE;

        glMultMatrixf(matrix);

        if (m.model) {
            ApplyRotationCorrectionIfNeeded(m.model, matrix);
            SetupMaterialFromColor(m.color);
            m.model->render();
        }
        else {
            SetupMaterialFromColor(m.color);
            DrawWireframeCube(0.3f);
        }


        glPopMatrix();
    }

    const auto& groups = SceneDataManager::Instance().GetGroupObjects();
    for (const auto& [uuid, g] : groups) {
        glPushMatrix();
        ApplyScaledTransform(g.transformMatrix);
        // DrawWireframeCube(0.4f); // Group color
        glPopMatrix();
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

// Converts an aiQuaternion to Euler angles in degrees
aiVector3D Viewer3DController::QuaternionToEulerDegrees(const aiQuaternion& q)
{
    aiVector3D euler;

    // Roll (X-axis)
    double sinr_cosp = 2.0 * (q.w * q.x + q.y * q.z);
    double cosr_cosp = 1.0 - 2.0 * (q.x * q.x + q.y * q.y);
    euler.x = std::atan2(sinr_cosp, cosr_cosp);

    // Pitch (Y-axis)
    double sinp = 2.0 * (q.w * q.y - q.z * q.x);
    if (std::abs(sinp) >= 1.0)
        euler.y = std::copysign(M_PI / 2.0, sinp); // Use 90 degrees if out of range
    else
        euler.y = std::asin(sinp);

    // Yaw (Z-axis)
    double siny_cosp = 2.0 * (q.w * q.z + q.x * q.y);
    double cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
    euler.z = std::atan2(siny_cosp, cosy_cosp);

    // Convert radians to degrees
    constexpr double RAD2DEG = 180.0 / M_PI;
    euler.x *= RAD2DEG;
    euler.y *= RAD2DEG;
    euler.z *= RAD2DEG;

    return euler;
}

// Applies rotation correction based on the transform matrix and model size
void Viewer3DController::ApplyRotationCorrectionIfNeeded(const std::shared_ptr<ThreeModelAssimp>& model, const float matrix[16])
{
    if (!model) return;

    aiMatrix4x4 trf(
        matrix[0], matrix[1], matrix[2], matrix[3],
        matrix[4], matrix[5], matrix[6], matrix[7],
        matrix[8], matrix[9], matrix[10], matrix[11],
        matrix[12], matrix[13], matrix[14], matrix[15]
    );

    aiVector3D scale, position;
    aiQuaternion rotation;
    trf.Decompose(scale, rotation, position);

    constexpr float epsilon = 0.001f;
    bool isRotated =
        std::abs(rotation.x) > epsilon ||
        std::abs(rotation.y) > epsilon ||
        std::abs(rotation.z) > epsilon;

    if (isRotated) {
        aiVector3D size = model->getSize();
        aiVector3D correction(0.0f, 0.0f, 0.0f);

        if (std::abs(rotation.z) > std::abs(rotation.x) && std::abs(rotation.z) > std::abs(rotation.y)) {
            correction.y = -size.y;
        }
        else if (std::abs(rotation.x) > std::abs(rotation.y)) {
            correction.z = -size.z;
        }
        else {
            correction.x = -size.x;
        }

        glTranslatef(
            correction.x * RENDER_SCALE,
            correction.y * RENDER_SCALE,
            correction.z * RENDER_SCALE
        );
    }
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


void Viewer3DController::SetupMaterialFromColor(const aiColor3D& color) {
    SetupMaterialFromRGB(color.r, color.g, color.b);
}

void Viewer3DController::SetupMaterialFromRGB(float r, float g, float b) {
    glColor3f(r, g, b);
}

