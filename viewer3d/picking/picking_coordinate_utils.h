#pragma once

// Converts window mouse coordinates to validated OpenGL framebuffer coordinates.
bool TryConvertMouseToFramebufferPoint(int mouseX, int mouseY,
                                       int framebufferWidth,
                                       int framebufferHeight,
                                       int &framebufferX,
                                       int &framebufferY);
