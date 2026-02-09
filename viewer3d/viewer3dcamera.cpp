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
/*
 * File: viewer3dcamera.cpp
 * Author: Luisma Peramato
 * License: GNU General Public License v3.0
 * Description: Implementation of an orbital 3D camera.
 */

#include "viewer3dcamera.h"
#include <cmath>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
// macOS uses the OpenGL framework headers; choose the correct GLU header by platform.
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/glu.h>
#else
#include <GL/glu.h>
#endif

Viewer3DCamera::Viewer3DCamera()
    : yaw(0.0f), pitch(20.0f), distance(30.0f),
    targetX(0.0f), targetY(0.0f), targetZ(0.0f),
    minDistance(0.5f), maxDistance(500.0f)
{
    targetYaw = yaw;
    targetPitch = pitch;
    targetDistance = distance;
    targetTargetX = targetX;
    targetTargetY = targetY;
    targetTargetZ = targetZ;
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
    targetYaw += deltaYaw;
    targetPitch += deltaPitch;

    // Clamp pitch to avoid flipping
    if (targetPitch > 89.0f) targetPitch = 89.0f;
    if (targetPitch < -89.0f) targetPitch = -89.0f;
}

// Adjusts distance
void Viewer3DCamera::Zoom(float deltaSteps)
{
    // Use exponential zoom scale. Increase sensitivity when the camera
    // is far from the target so wheel scrolling covers large distances
    // more quickly.
    float base = 1.1f + 0.1f *
                 std::clamp(targetDistance / 200.0f, 0.0f, 1.0f);
    float factor = std::pow(base, deltaSteps);

    float newDistance = targetDistance * factor;

    if (newDistance < minDistance)
    {
        // Continue moving forward once we reach the minimum distance
        // by translating the target in the viewing direction so that
        // zooming in keeps advancing the camera.
        float radYaw = targetYaw * 3.14159265f / 180.0f;
        float radPitch = targetPitch * 3.14159265f / 180.0f;

        float forwardX = -cosf(radPitch) * sinf(radYaw);
        float forwardY =  cosf(radPitch) * cosf(radYaw);
        float forwardZ = -sinf(radPitch);

        float overshoot = minDistance - newDistance;

        targetTargetX += overshoot * forwardX;
        targetTargetY += overshoot * forwardY;
        targetTargetZ += overshoot * forwardZ;

        targetDistance = minDistance;
    }
    else
    {
        targetDistance = newDistance;
        if (targetDistance > maxDistance) targetDistance = maxDistance;
    }
}



// Moves the target point laterally (pan)
void Viewer3DCamera::Pan(float deltaX, float deltaY)
{
    float radYaw = targetYaw * 3.14159265f / 180.0f;

    float rightX = cosf(radYaw);
    float rightY = sinf(radYaw);

    targetTargetX += deltaX * rightX;
    targetTargetY += deltaX * rightY;
    targetTargetZ += deltaY;

}

void Viewer3DCamera::SetDistance(float d)
{
    distance = d;
    if (distance < minDistance) distance = minDistance;
    if (distance > maxDistance) distance = maxDistance;
    targetDistance = distance;
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
    targetYaw = yaw;
    targetPitch = pitch;
}

void Viewer3DCamera::Update(float dt)
{
    const float smoothing = 10.0f;
    const float alpha = std::clamp(dt * smoothing, 0.0f, 1.0f);

    yaw += (targetYaw - yaw) * alpha;
    pitch += (targetPitch - pitch) * alpha;
    distance += (targetDistance - distance) * alpha;
    targetX += (targetTargetX - targetX) * alpha;
    targetY += (targetTargetY - targetY) * alpha;
    targetZ += (targetTargetZ - targetZ) * alpha;
}

void Viewer3DCamera::Reset()
{
    yaw = 45.0f;
    pitch = 30.0f;
    distance = 15.0f;
    targetX = targetY = targetZ = 0.0f;

    targetYaw = yaw;
    targetPitch = pitch;
    targetDistance = distance;
    targetTargetX = targetX;
    targetTargetY = targetY;
    targetTargetZ = targetZ;
}
