#pragma once

#include <string>

#include <GL/glew.h>

namespace glcapture {

class FramebufferCaptureTarget {
public:
  FramebufferCaptureTarget() = default;
  ~FramebufferCaptureTarget();

  FramebufferCaptureTarget(const FramebufferCaptureTarget &) = delete;
  FramebufferCaptureTarget &operator=(const FramebufferCaptureTarget &) = delete;

  FramebufferCaptureTarget(FramebufferCaptureTarget &&other) noexcept;
  FramebufferCaptureTarget &operator=(FramebufferCaptureTarget &&other) noexcept;

  // Creates the framebuffer, color texture, and depth/stencil attachment.
  bool Initialize(int width, int height);

  // Binds the framebuffer so callers can render into the capture target.
  void BindForRendering() const;

  // Binds the color attachment as the source for pixel reads.
  void BindForReading() const;

  // Returns whether the framebuffer was created and checked successfully.
  bool IsComplete() const { return complete_; }

  // Returns the target width in pixels.
  int Width() const { return width_; }

  // Returns the target height in pixels.
  int Height() const { return height_; }

  // Returns the most recent creation or completeness diagnostic.
  const std::string &Diagnostic() const { return diagnostic_; }

private:
  // Releases any OpenGL objects owned by this target.
  void Reset();

  GLuint framebuffer_ = 0;
  GLuint colorTexture_ = 0;
  GLuint depthStencilRenderbuffer_ = 0;
  int width_ = 0;
  int height_ = 0;
  bool complete_ = false;
  std::string diagnostic_;
};

} // namespace glcapture
