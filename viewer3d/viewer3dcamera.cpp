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
#include "navigation_diagnostics.h"
#include <cmath>
#include <algorithm>
#include <wx/debug.h>

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

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kDefaultYawDegrees = 45.0f;
constexpr float kDefaultPitchDegrees = 30.0f;
constexpr float kDefaultDistance = 15.0f;

// Returns whether a scalar is finite enough to be used in camera math.
bool IsFiniteCameraValue(float value)
{
    return std::isfinite(value);
}

// Wraps yaw to a bounded range so repeated input cannot grow it without limit.
float NormalizeYawDegrees(float value)
{
    if (!IsFiniteCameraValue(value))
        return 0.0f;
    value = std::fmod(value, 360.0f);
    if (value > 180.0f)
        value -= 360.0f;
    if (value < -180.0f)
        value += 360.0f;
    return value;
}

// Clamps pitch away from the singularities at straight up and straight down.
float ClampPitchDegrees(float value)
{
    if (!IsFiniteCameraValue(value))
        return 0.0f;
    return std::clamp(value, -89.0f, 89.0f);
}
} // namespace

// Initializes the orbital camera with deterministic default state.
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
    SanitizeTargetState("constructor");
}

// Releases camera resources owned by the camera object.
Viewer3DCamera::~Viewer3DCamera() = default;

// Applies the current camera transform to the active OpenGL model-view matrix.
void Viewer3DCamera::Apply() const
{
    if (!IsValid()) {
        viewer3d::diagnostics::Log("Viewer3DCamera::Apply rejected invalid camera state.");
        wxASSERT_MSG(false, "Invalid 3D camera state before gluLookAt.");
        return;
    }

    const float radYaw = yaw * kPi / 180.0f;
    const float radPitch = pitch * kPi / 180.0f;

    const float x = distance * cosf(radPitch) * sinf(radYaw);
    const float z = distance * sinf(radPitch);
    const float y = -distance * cosf(radPitch) * cosf(radYaw);

    const float camX = targetX + x;
    const float camY = targetY + y;
    const float camZ = targetZ + z;
    if (!IsFiniteCameraValue(camX) || !IsFiniteCameraValue(camY) ||
        !IsFiniteCameraValue(camZ)) {
        viewer3d::diagnostics::Log("Viewer3DCamera::Apply produced invalid eye coordinates.");
        wxASSERT_MSG(false, "Invalid 3D camera eye coordinates before gluLookAt.");
        return;
    }

    gluLookAt(camX, camY, camZ, targetX, targetY, targetZ, 0, 0, 1);
}

// Offsets yaw and pitch targets for orbit navigation.
void Viewer3DCamera::Orbit(float deltaYaw, float deltaPitch)
{
    if (!IsFiniteCameraValue(deltaYaw) || !IsFiniteCameraValue(deltaPitch)) {
        viewer3d::diagnostics::Log("Viewer3DCamera::Orbit ignored non-finite input delta.");
        wxASSERT_MSG(false, "Non-finite orbit delta.");
        return;
    }
    targetYaw += deltaYaw;
    targetPitch += deltaPitch;
    SanitizeTargetState("Orbit");
    viewer3d::diagnostics::Logf(
        "Camera orbit target yaw=%.3f pitch=%.3f", targetYaw, targetPitch);
}

// Changes the camera target distance and advances through the minimum distance when zooming in.
void Viewer3DCamera::Zoom(float deltaSteps)
{
    if (!IsFiniteCameraValue(deltaSteps)) {
        viewer3d::diagnostics::Log("Viewer3DCamera::Zoom ignored non-finite input delta.");
        wxASSERT_MSG(false, "Non-finite zoom delta.");
        return;
    }
    deltaSteps = std::clamp(deltaSteps, -64.0f, 64.0f);

    const float base = 1.1f + 0.1f *
                 std::clamp(targetDistance / 200.0f, 0.0f, 1.0f);
    const float factor = std::pow(base, deltaSteps);
    if (!IsFiniteCameraValue(factor)) {
        viewer3d::diagnostics::Log("Viewer3DCamera::Zoom rejected non-finite zoom factor.");
        wxASSERT_MSG(false, "Non-finite zoom factor.");
        return;
    }

    const float newDistance = targetDistance * factor;
    if (!IsFiniteCameraValue(newDistance)) {
        targetDistance = deltaSteps > 0.0f ? maxDistance : minDistance;
        viewer3d::diagnostics::Log("Viewer3DCamera::Zoom clamped non-finite distance.");
        return;
    }

    if (newDistance < minDistance)
    {
        const float radYaw = targetYaw * kPi / 180.0f;
        const float radPitch = targetPitch * kPi / 180.0f;

        const float forwardX = -cosf(radPitch) * sinf(radYaw);
        const float forwardY =  cosf(radPitch) * cosf(radYaw);
        const float forwardZ = -sinf(radPitch);

        const float overshoot = minDistance - newDistance;

        targetTargetX += overshoot * forwardX;
        targetTargetY += overshoot * forwardY;
        targetTargetZ += overshoot * forwardZ;

        targetDistance = minDistance;
    }
    else
    {
        targetDistance = std::clamp(newDistance, minDistance, maxDistance);
    }
    SanitizeTargetState("Zoom");
    viewer3d::diagnostics::Logf(
        "Camera zoom target distance=%.3f target=(%.3f, %.3f, %.3f)",
        targetDistance, targetTargetX, targetTargetY, targetTargetZ);
}

// Moves the camera target laterally in the current horizontal view basis.
void Viewer3DCamera::Pan(float deltaX, float deltaY)
{
    if (!IsFiniteCameraValue(deltaX) || !IsFiniteCameraValue(deltaY)) {
        viewer3d::diagnostics::Log("Viewer3DCamera::Pan ignored non-finite input delta.");
        wxASSERT_MSG(false, "Non-finite pan delta.");
        return;
    }

    const float radYaw = targetYaw * kPi / 180.0f;

    const float rightX = cosf(radYaw);
    const float rightY = sinf(radYaw);

    targetTargetX += deltaX * rightX;
    targetTargetY += deltaX * rightY;
    targetTargetZ += deltaY;
    SanitizeTargetState("Pan");
    viewer3d::diagnostics::Logf(
        "Camera pan target=(%.3f, %.3f, %.3f)",
        targetTargetX, targetTargetY, targetTargetZ);
}

// Sets the camera distance while enforcing configured distance bounds.
void Viewer3DCamera::SetDistance(float d)
{
    if (!IsFiniteCameraValue(d)) {
        viewer3d::diagnostics::Log("Viewer3DCamera::SetDistance received non-finite distance.");
        wxASSERT_MSG(false, "Non-finite camera distance.");
        d = kDefaultDistance;
    }
    distance = std::clamp(d, minDistance, maxDistance);
    targetDistance = distance;
    SanitizeTargetState("SetDistance");
}

// Returns the current camera distance from its target point.
float Viewer3DCamera::GetDistance() const
{
    return distance;
}

// Sets the current and target camera orientation angles.
void Viewer3DCamera::SetOrientation(float y, float p)
{
    yaw = NormalizeYawDegrees(y);
    pitch = ClampPitchDegrees(p);
    targetYaw = yaw;
    targetPitch = pitch;
    SanitizeTargetState("SetOrientation");
}


// Sets the current and target camera look-at point.
void Viewer3DCamera::SetTarget(float x, float y, float z)
{
    targetX = x;
    targetY = y;
    targetZ = z;
    targetTargetX = x;
    targetTargetY = y;
    targetTargetZ = z;
    SanitizeTargetState("SetTarget");
}

// Synchronizes current camera values to the latest input-driven target values.
void Viewer3DCamera::Update(float dt)
{
    (void)dt;
    SanitizeTargetState("UpdateBeforeApply");

    yaw = targetYaw;
    pitch = targetPitch;
    distance = targetDistance;
    targetX = targetTargetX;
    targetY = targetTargetY;
    targetZ = targetTargetZ;
    SanitizeTargetState("UpdateAfterApply");
    viewer3d::diagnostics::Logf(
        "Camera update yaw=%.3f pitch=%.3f distance=%.3f target=(%.3f, %.3f, %.3f)",
        yaw, pitch, distance, targetX, targetY, targetZ);
}

// Reports whether all active and target camera values are finite and usable.
bool Viewer3DCamera::IsValid() const
{
    return IsFiniteCameraValue(yaw) && IsFiniteCameraValue(pitch) &&
           IsFiniteCameraValue(distance) && IsFiniteCameraValue(targetX) &&
           IsFiniteCameraValue(targetY) && IsFiniteCameraValue(targetZ) &&
           IsFiniteCameraValue(targetYaw) && IsFiniteCameraValue(targetPitch) &&
           IsFiniteCameraValue(targetDistance) && IsFiniteCameraValue(targetTargetX) &&
           IsFiniteCameraValue(targetTargetY) && IsFiniteCameraValue(targetTargetZ) &&
           distance >= minDistance && distance <= maxDistance &&
           targetDistance >= minDistance && targetDistance <= maxDistance;
}

// Returns a compact camera state snapshot for diagnostics and fingerprints.
std::array<float, 12> Viewer3DCamera::GetStateForDiagnostics() const
{
    return {yaw, pitch, distance, targetX, targetY, targetZ, targetYaw,
            targetPitch, targetDistance, targetTargetX, targetTargetY,
            targetTargetZ};
}

// Resets camera to the default isometric navigation state.
void Viewer3DCamera::Reset()
{
    yaw = kDefaultYawDegrees;
    pitch = kDefaultPitchDegrees;
    distance = kDefaultDistance;
    targetX = targetY = targetZ = 0.0f;

    targetYaw = yaw;
    targetPitch = pitch;
    targetDistance = distance;
    targetTargetX = targetX;
    targetTargetY = targetY;
    targetTargetZ = targetZ;
    SanitizeTargetState("Reset");
}

// Normalizes target values and restores safe defaults when invalid values are detected.
void Viewer3DCamera::SanitizeTargetState(const char* source)
{
    bool corrected = false;
    auto replaceIfInvalid = [&](float& value, float replacement) {
        if (!IsFiniteCameraValue(value)) {
            value = replacement;
            corrected = true;
        }
    };

    replaceIfInvalid(yaw, kDefaultYawDegrees);
    replaceIfInvalid(pitch, kDefaultPitchDegrees);
    replaceIfInvalid(distance, kDefaultDistance);
    replaceIfInvalid(targetX, 0.0f);
    replaceIfInvalid(targetY, 0.0f);
    replaceIfInvalid(targetZ, 0.0f);
    replaceIfInvalid(targetYaw, yaw);
    replaceIfInvalid(targetPitch, pitch);
    replaceIfInvalid(targetDistance, distance);
    replaceIfInvalid(targetTargetX, targetX);
    replaceIfInvalid(targetTargetY, targetY);
    replaceIfInvalid(targetTargetZ, targetZ);

    yaw = NormalizeYawDegrees(yaw);
    targetYaw = NormalizeYawDegrees(targetYaw);
    pitch = ClampPitchDegrees(pitch);
    targetPitch = ClampPitchDegrees(targetPitch);
    distance = std::clamp(distance, minDistance, maxDistance);
    targetDistance = std::clamp(targetDistance, minDistance, maxDistance);

    if (corrected) {
        viewer3d::diagnostics::Logf(
            "Viewer3DCamera corrected invalid state after %s.", source ? source : "unknown");
        wxASSERT_MSG(false, "Corrected invalid 3D camera state.");
    }
}
