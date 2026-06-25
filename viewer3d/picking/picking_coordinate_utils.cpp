#include "picking_coordinate_utils.h"

// Converts a top-left-origin mouse point into bottom-left-origin framebuffer coordinates.
bool TryConvertMouseToFramebufferPoint(int mouseX, int mouseY,
                                       int framebufferWidth,
                                       int framebufferHeight,
                                       int &framebufferX,
                                       int &framebufferY) {
  if (framebufferWidth <= 0 || framebufferHeight <= 0)
    return false;
  if (mouseX < 0 || mouseY < 0 || mouseX >= framebufferWidth ||
      mouseY >= framebufferHeight) {
    return false;
  }

  framebufferX = mouseX;
  framebufferY = framebufferHeight - 1 - mouseY;
  return framebufferX >= 0 && framebufferY >= 0 &&
         framebufferX < framebufferWidth && framebufferY < framebufferHeight;
}
