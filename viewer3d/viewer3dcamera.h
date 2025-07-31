/*
 * File: viewer3dcamera.h
 * Author: Luisma Peramato
 * License: MIT
 * Description: Orbital camera for 3D navigation around a target point.
 */

#pragma once

class Viewer3DCamera
{
public:
    Viewer3DCamera();
    ~Viewer3DCamera();

    // Applies the view transformation using gluLookAt
    void Apply() const;

    // Adjusts yaw and pitch angles
    void Orbit(float deltaYaw, float deltaPitch);

    // Zooms in or out. When zooming in beyond the minimum
    // distance the camera keeps moving forward in the
    // viewing direction.
    void Zoom(float deltaDistance);

    // Moves the target point laterally (pan)
    void Pan(float deltaX, float deltaY);

    // Sets yaw and pitch directly
    void SetOrientation(float y, float p);

    // Sets the distance directly
    void SetDistance(float d);

    // Returns current distance
    float GetDistance() const;

    float GetYaw() const { return yaw; }
    float GetPitch() const { return pitch; }
    float GetTargetX() const { return targetX; }
    float GetTargetY() const { return targetY; }
    float GetTargetZ() const { return targetZ; }
    void SetTarget(float x, float y, float z) { targetX = x; targetY = y; targetZ = z; }

    // Resets camera to default orientation and target
    void Reset();

private:
    float yaw;       // Horizontal angle in degrees
    float pitch;     // Vertical angle in degrees
    float distance;  // Distance from target

    float targetX;   // Pan offset X
    float targetY;   // Pan offset Y
    float targetZ;   // Pan offset Z

    float minDistance;
    float maxDistance;
};
