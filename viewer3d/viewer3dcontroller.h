/*
 * File: viewer3dcontroller.h
 * Author: Luisma Peramato
 * License: MIT
 * Description: Controller class for 3D viewer logic and state.
 */

#pragma once

#include "scenedatamanager.h"


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

    // Initializes simple lighting for the scene
    void SetupBasicLighting();
    void SetupMaterialFromRGB(float r, float g, float b);
};
