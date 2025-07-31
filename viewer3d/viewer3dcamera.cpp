/*
 * File: viewer3dcamera.cpp
 * Author: Luisma Peramato
 * License: MIT
 * Description: Implementation of an orbital 3D camera.
 */

#include "viewer3dcamera.h"
#include <cmath>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <GL/glu.h>

Viewer3DCamera::Viewer3DCamera()
    : yaw(0.0f), pitch(20.0f), distance(30.0f),
    targetX(0.0f), targetY(0.0f), targetZ(0.0f),
    minDistance(0.5f), maxDistance(500.0f)
{
}


Viewer3DCamera::~Viewer3DCamera() = default;

// Applies the camera transformation
void Viewer3DCamera::Apply() const
{
    float radYaw = yaw * 3.14159265f / 180.0f;
    float radPitch = pitch * 3.14159265f / 180.0f;

    float x = distance * cosf(radPitch) * sinf(radYaw);
    float z = distance * sinf(radPitch);
    float y = -distance * cosf(radPitch) * cosf(radYaw);

    // Compute camera position relative to the target
    float camX = targetX + x;
    float camY = targetY + y;
    float camZ = targetZ + z;

    gluLookAt(camX, camY, camZ,  // eye position
        targetX, targetY, targetZ,  // look-at point
        0, 0, 1);  // Z is up
}


// Adjusts yaw and pitch
void Viewer3DCamera::Orbit(float deltaYaw, float deltaPitch)
{
    yaw += deltaYaw;
    pitch += deltaPitch;

    // Clamp pitch to avoid flipping
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}

// Adjusts distance
void Viewer3DCamera::Zoom(float deltaSteps)
{
    // Use exponential zoom scale. Increase sensitivity when the camera
    // is far from the target so wheel scrolling covers large distances
    // more quickly.
    float base = 1.1f + 0.1f *
                 std::clamp(distance / 200.0f, 0.0f, 1.0f);
    float factor = std::pow(base, deltaSteps);

    float newDistance = distance * factor;

    if (newDistance < minDistance)
    {
        // Continue moving forward once we reach the minimum distance
        // by translating the target in the viewing direction so that
        // zooming in keeps advancing the camera.
        float radYaw = yaw * 3.14159265f / 180.0f;
        float radPitch = pitch * 3.14159265f / 180.0f;

        float forwardX = -cosf(radPitch) * sinf(radYaw);
        float forwardY =  cosf(radPitch) * cosf(radYaw);
        float forwardZ = -sinf(radPitch);

        float overshoot = minDistance - newDistance;

        targetX += overshoot * forwardX;
        targetY += overshoot * forwardY;
        targetZ += overshoot * forwardZ;

        distance = minDistance;
    }
    else
    {
        distance = newDistance;
        if (distance > maxDistance) distance = maxDistance;
    }
}



// Moves the target point laterally (pan)
void Viewer3DCamera::Pan(float deltaX, float deltaY)
{
    float radYaw = yaw * 3.14159265f / 180.0f;

    float rightX = cosf(radYaw);
    float rightY = sinf(radYaw);

    targetX += deltaX * rightX;
    targetY += deltaX * rightY;
    targetZ += deltaY;

}

void Viewer3DCamera::SetDistance(float d)
{
    distance = d;
    if (distance < minDistance) distance = minDistance;
    if (distance > maxDistance) distance = maxDistance;
}

float Viewer3DCamera::GetDistance() const
{
    return distance;
}

void Viewer3DCamera::SetOrientation(float y, float p)
{
    yaw = y;
    pitch = p;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}

void Viewer3DCamera::Reset()
{
    yaw = 45.0f;
    pitch = 30.0f;
    distance = 15.0f;
    targetX = targetY = targetZ = 0.0f;
}
