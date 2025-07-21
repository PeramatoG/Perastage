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

    // Zooms in or out
    void Zoom(float deltaDistance);

    // Moves the target point laterally (pan)
    void Pan(float deltaX, float deltaY);

    // Sets the distance directly
    void SetDistance(float d);

    // Returns current distance
    float GetDistance() const;

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
