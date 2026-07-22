#include "gl_framebuffer_capture_target.h"

#include <iterator>
#include <sstream>
#include <utility>

namespace glcapture {
namespace {

class ScopedTextureRenderbufferBindingState {
public:
  // Captures texture and renderbuffer bindings changed during target creation.
  ScopedTextureRenderbufferBindingState() {
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding_);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbufferBinding_);
  }

  ScopedTextureRenderbufferBindingState(
      const ScopedTextureRenderbufferBindingState &) = delete;
  ScopedTextureRenderbufferBindingState &
  operator=(const ScopedTextureRenderbufferBindingState &) = delete;

  // Restores texture and renderbuffer bindings after target creation.
  ~ScopedTextureRenderbufferBindingState() {
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(textureBinding_));
    glBindRenderbuffer(GL_RENDERBUFFER,
                       static_cast<GLuint>(renderbufferBinding_));
  }

private:
  GLint textureBinding_ = 0;
  GLint renderbufferBinding_ = 0;
};

// Formats an OpenGL framebuffer status code for diagnostics.
std::string FormatFramebufferStatus(GLenum status) {
  std::ostringstream stream;
  stream << "OpenGL framebuffer is incomplete (status 0x" << std::hex
         << status << ")";
  return stream.str();
}

} // namespace

// Deletes OpenGL resources owned by the framebuffer capture target.
FramebufferCaptureTarget::~FramebufferCaptureTarget() { Reset(); }

// Transfers framebuffer ownership from another capture target.
FramebufferCaptureTarget::FramebufferCaptureTarget(
    FramebufferCaptureTarget &&other) noexcept {
  *this = std::move(other);
}

// Replaces this capture target with resources owned by another target.
FramebufferCaptureTarget &FramebufferCaptureTarget::operator=(
    FramebufferCaptureTarget &&other) noexcept {
  if (this == &other)
    return *this;

  Reset();
  framebuffer_ = other.framebuffer_;
  colorTexture_ = other.colorTexture_;
  depthStencilRenderbuffer_ = other.depthStencilRenderbuffer_;
  width_ = other.width_;
  height_ = other.height_;
  allocationCount_ = other.allocationCount_;
  complete_ = other.complete_;
  diagnostic_ = std::move(other.diagnostic_);

  other.framebuffer_ = 0;
  other.colorTexture_ = 0;
  other.depthStencilRenderbuffer_ = 0;
  other.width_ = 0;
  other.height_ = 0;
  other.allocationCount_ = 0;
  other.complete_ = false;
  other.diagnostic_.clear();
  return *this;
}

// Creates or reuses framebuffer resources when the requested size is unchanged.
bool FramebufferCaptureTarget::EnsureSize(int width, int height) {
  if (complete_ && width_ == width && height_ == height) {
    diagnostic_.clear();
    return true;
  }
  return Initialize(width, height);
}

// Creates the framebuffer, color texture, and depth/stencil attachment.
bool FramebufferCaptureTarget::Initialize(int width, int height) {
  Reset();
  width_ = width;
  height_ = height;

  if (width <= 0 || height <= 0) {
    diagnostic_ = "Framebuffer capture target size must be positive";
    return false;
  }

  ScopedTextureRenderbufferBindingState bindingStateGuard;

  glGenFramebuffers(1, &framebuffer_);
  if (framebuffer_ == 0) {
    diagnostic_ = "OpenGL did not create a framebuffer object";
    return false;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

  glGenTextures(1, &colorTexture_);
  if (colorTexture_ == 0) {
    diagnostic_ = "OpenGL did not create a framebuffer color texture";
    Reset();
    return false;
  }
  glBindTexture(GL_TEXTURE_2D, colorTexture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         colorTexture_, 0);

  glGenRenderbuffers(1, &depthStencilRenderbuffer_);
  if (depthStencilRenderbuffer_ == 0) {
    diagnostic_ = "OpenGL did not create a framebuffer depth/stencil buffer";
    Reset();
    return false;
  }
  glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRenderbuffer_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width_, height_);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER, depthStencilRenderbuffer_);

  const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    diagnostic_ = FormatFramebufferStatus(status);
    Reset();
    return false;
  }

  complete_ = true;
  ++allocationCount_;
  diagnostic_.clear();
  return true;
}

// Binds the framebuffer so callers can render into the capture target.
void FramebufferCaptureTarget::BindForRendering() const {
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
}

// Binds the framebuffer and color attachment as the source for pixel reads.
void FramebufferCaptureTarget::BindForReading() const {
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
}

// Releases all OpenGL resources owned by this target.
void FramebufferCaptureTarget::Release() { Reset(); }

// Releases any OpenGL objects owned by this target.
void FramebufferCaptureTarget::Reset() {
  if (depthStencilRenderbuffer_ != 0)
    glDeleteRenderbuffers(1, &depthStencilRenderbuffer_);
  if (colorTexture_ != 0)
    glDeleteTextures(1, &colorTexture_);
  if (framebuffer_ != 0)
    glDeleteFramebuffers(1, &framebuffer_);

  framebuffer_ = 0;
  colorTexture_ = 0;
  depthStencilRenderbuffer_ = 0;
  width_ = 0;
  height_ = 0;
  complete_ = false;
}

// Creates a bounded framebuffer target cache.
FramebufferCaptureTargetCache::FramebufferCaptureTargetCache(size_t capacity)
    : capacity_(capacity == 0 ? 1 : capacity) {}

// Releases cached OpenGL resources before the cache is destroyed.
FramebufferCaptureTargetCache::~FramebufferCaptureTargetCache() { Release(); }

// Returns a complete target for the requested size and records cache diagnostics.
FramebufferCaptureTarget *FramebufferCaptureTargetCache::Acquire(int width, int height) {
  const SizeKey key{width, height};
  const auto found = index_.find(key);
  if (found != index_.end()) {
    entries_.splice(entries_.begin(), entries_, found->second);
    ++stats_.hits;
    diagnostic_.clear();
    return &entries_.front().target;
  }

  ++stats_.misses;
  CacheEntry created;
  created.key = key;
  if (!created.target.EnsureSize(width, height) || !created.target.IsComplete()) {
    diagnostic_ = created.target.Diagnostic();
    return nullptr;
  }
  stats_.allocations += created.target.AllocationCount();

  entries_.push_front(std::move(created));
  index_[key] = entries_.begin();
  while (entries_.size() > capacity_) {
    auto last = std::prev(entries_.end());
    index_.erase(last->key);
    last->target.Release();
    entries_.erase(last);
    ++stats_.evictions;
  }
  stats_.entries = static_cast<int>(entries_.size());
  diagnostic_.clear();
  return &entries_.front().target;
}

// Releases all cached targets and clears lookup state.
void FramebufferCaptureTargetCache::Release() {
  for (auto &entry : entries_)
    entry.target.Release();
  entries_.clear();
  index_.clear();
  stats_.entries = 0;
}

// Returns a snapshot of current cache counters.
FramebufferCaptureCacheStats FramebufferCaptureTargetCache::Stats() const {
  FramebufferCaptureCacheStats stats = stats_;
  stats.entries = static_cast<int>(entries_.size());
  return stats;
}

} // namespace glcapture
