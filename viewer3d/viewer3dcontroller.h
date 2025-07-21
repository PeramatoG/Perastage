/*
 * File: viewer3dcontroller.h
 * Author: Luisma Peramato
 * License: MIT
 * Description: Controller class for 3D viewer logic and state.
 */

#pragma once

#include <memory>
#include <assimp/quaternion.h>
#include <assimp/vector3.h>
#include <assimp/types.h>


class ThreeModelAssimp;  // Forward declaration

class Viewer3DController
{
public:
    Viewer3DController();
    ~Viewer3DController();

    // Updates the internal logic if needed (e.g. animation, camera)
    void Update();

    // Renders all scene objects
    void RenderScene();

private:
    // Draws a solid cube centered at origin with given size and color
    void DrawCube(float size = 0.2f, float r = 1.0f, float g = 1.0f, float b = 1.0f);

    // Draws a wireframe cube centered at origin with given size
    void DrawWireframeCube(float size = 0.3f);

    // Applies uniform scaling and then the object's transformation matrix
    void ApplyScaledTransform(const float matrix[16]);

    // Draws the reference grid on the Z=0 plane
    void DrawGrid();

    // Draws the XYZ axis lines
    void DrawAxes();

    // Converts an aiQuaternion to Euler angles (in degrees)
    aiVector3D QuaternionToEulerDegrees(const aiQuaternion& q);

    // Applies rotation correction for model rendering based on transform matrix
    void ApplyRotationCorrectionIfNeeded(const std::shared_ptr<ThreeModelAssimp>& model, const float matrix[16]);

    // Initializes simple lighting for the scene
    void SetupBasicLighting();

    // Enables material rendering from aiColor3D
    void SetupMaterialFromColor(const aiColor3D& color);

    // Enables material rendering from float RGB values
    void SetupMaterialFromRGB(float r, float g, float b);
};
