#pragma once

struct SimpleCamera {
    float x = 0.0f;
    float y = 0.0f;
    float z = 5.0f;
    float yaw = 0.0f;   // rotation around Z axis
    float pitch = 0.0f; // rotation around X axis
    float fov = 1.0f;   // vertical field of view in radians
};
