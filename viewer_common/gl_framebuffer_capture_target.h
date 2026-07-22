#pragma once

#include <cstddef>
#include <list>
#include <string>
#include <unordered_map>

#include <GL/glew.h>

namespace glcapture {

struct FramebufferCaptureCacheStats {
  int hits = 0;
  int misses = 0;
  int allocations = 0;
  int evictions = 0;
  int entries = 0;
};

class FramebufferCaptureTarget {
public:
  FramebufferCaptureTarget() = default;
  ~FramebufferCaptureTarget();

  FramebufferCaptureTarget(const FramebufferCaptureTarget &) = delete;
  FramebufferCaptureTarget &operator=(const FramebufferCaptureTarget &) = delete;

  FramebufferCaptureTarget(FramebufferCaptureTarget &&other) noexcept;
  FramebufferCaptureTarget &operator=(FramebufferCaptureTarget &&other) noexcept;

  // Creates or reuses the framebuffer resources for the requested size.
  bool EnsureSize(int width, int height);

  // Creates the framebuffer, color texture, and depth/stencil attachment.
  bool Initialize(int width, int height);

  // Releases all OpenGL resources owned by this target.
  void Release();

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

  // Returns how many framebuffer allocations were needed by this target.
  int AllocationCount() const { return allocationCount_; }

private:
  // Releases any OpenGL objects owned by this target.
  void Reset();

  GLuint framebuffer_ = 0;
  GLuint colorTexture_ = 0;
  GLuint depthStencilRenderbuffer_ = 0;
  int width_ = 0;
  int height_ = 0;
  int allocationCount_ = 0;
  bool complete_ = false;
  std::string diagnostic_;
};

class FramebufferCaptureTargetCache {
public:
  explicit FramebufferCaptureTargetCache(size_t capacity = 6);
  ~FramebufferCaptureTargetCache();

  FramebufferCaptureTargetCache(const FramebufferCaptureTargetCache &) = delete;
  FramebufferCaptureTargetCache &operator=(const FramebufferCaptureTargetCache &) = delete;

  // Returns a complete framebuffer target for the requested size when possible.
  FramebufferCaptureTarget *Acquire(int width, int height);

  // Releases all OpenGL resources owned by cached targets.
  void Release();

  // Returns cache hit, miss, allocation, eviction, and entry counters.
  FramebufferCaptureCacheStats Stats() const;

  // Returns the most recent creation or completeness diagnostic.
  const std::string &Diagnostic() const { return diagnostic_; }

private:
  struct SizeKey {
    int width = 0;
    int height = 0;

    bool operator==(const SizeKey &other) const {
      return width == other.width && height == other.height;
    }
  };

  struct SizeKeyHash {
    size_t operator()(const SizeKey &key) const {
      return (static_cast<size_t>(key.width) << 32) ^ static_cast<size_t>(key.height);
    }
  };

  struct CacheEntry {
    SizeKey key;
    FramebufferCaptureTarget target;
  };

  using EntryList = std::list<CacheEntry>;

  size_t capacity_ = 0;
  EntryList entries_;
  std::unordered_map<SizeKey, EntryList::iterator, SizeKeyHash> index_;
  FramebufferCaptureCacheStats stats_;
  std::string diagnostic_;
};

} // namespace glcapture
