#include "picking_coordinate_utils.h"

#include <cstdlib>
#include <iostream>

namespace {

// Verifies a valid mouse coordinate conversion result.
void ExpectConvert(int mouseX, int mouseY, int width, int height, int expectedX,
                   int expectedY) {
  int framebufferX = -1;
  int framebufferY = -1;
  if (!TryConvertMouseToFramebufferPoint(mouseX, mouseY, width, height,
                                         framebufferX, framebufferY)) {
    std::cerr << "Expected conversion to succeed for mouse=(" << mouseX << ','
              << mouseY << ") size=(" << width << ',' << height << ")\n";
    std::exit(1);
  }
  if (framebufferX != expectedX || framebufferY != expectedY) {
    std::cerr << "Unexpected framebuffer point: got=(" << framebufferX << ','
              << framebufferY << ") expected=(" << expectedX << ','
              << expectedY << ")\n";
    std::exit(1);
  }
}

// Verifies that an invalid mouse coordinate conversion is rejected.
void ExpectReject(int mouseX, int mouseY, int width, int height) {
  int framebufferX = 123;
  int framebufferY = 456;
  if (TryConvertMouseToFramebufferPoint(mouseX, mouseY, width, height,
                                        framebufferX, framebufferY)) {
    std::cerr << "Expected conversion to fail for mouse=(" << mouseX << ','
              << mouseY << ") size=(" << width << ',' << height << ")\n";
    std::exit(1);
  }
}

} // namespace

// Exercises edge and invalid coordinate conversion cases.
int main() {
  ExpectConvert(0, 0, 640, 480, 0, 479);
  ExpectConvert(0, 479, 640, 480, 0, 0);
  ExpectConvert(639, 0, 640, 480, 639, 479);
  ExpectConvert(639, 479, 640, 480, 639, 0);

  ExpectReject(-1, 0, 640, 480);
  ExpectReject(0, -1, 640, 480);
  ExpectReject(640, 0, 640, 480);
  ExpectReject(0, 480, 640, 480);
  ExpectReject(0, 0, 0, 480);
  ExpectReject(0, 0, 640, 0);

  return 0;
}
