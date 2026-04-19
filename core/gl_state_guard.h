#pragma once

#include <array>

#include <GL/glew.h>

namespace glstate {

class ScopedFramebufferViewportScissorState {
public:
  ScopedFramebufferViewportScissorState() {
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer_);
    glGetIntegerv(GL_VIEWPORT, viewport_.data());
    scissorEnabled_ = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
    glGetIntegerv(GL_SCISSOR_BOX, scissorBox_.data());
  }

  ScopedFramebufferViewportScissorState(
      const ScopedFramebufferViewportScissorState &) = delete;
  ScopedFramebufferViewportScissorState &
  operator=(const ScopedFramebufferViewportScissorState &) = delete;

  ~ScopedFramebufferViewportScissorState() {
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer_));
    glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
    if (scissorEnabled_) {
      glEnable(GL_SCISSOR_TEST);
      glScissor(scissorBox_[0], scissorBox_[1], scissorBox_[2], scissorBox_[3]);
    } else {
      glDisable(GL_SCISSOR_TEST);
    }
  }

private:
  GLint framebuffer_ = 0;
  std::array<GLint, 4> viewport_{0, 0, 0, 0};
  std::array<GLint, 4> scissorBox_{0, 0, 0, 0};
  bool scissorEnabled_ = false;
};

inline void ApplyKnownBaseOnscreenState(int width, int height) {
  glDisable(GL_SCISSOR_TEST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, width, height);
}

} // namespace glstate

