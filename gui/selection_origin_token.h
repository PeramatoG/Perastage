#pragma once

namespace selection {

enum class Origin {
  None,
  Viewer2D,
  Table,
  Viewer3D,
};

inline Origin &OriginStorage() {
  static thread_local Origin origin = Origin::None;
  return origin;
}

class ScopedOrigin {
public:
  explicit ScopedOrigin(Origin origin) : m_previous(OriginStorage()) {
    OriginStorage() = origin;
  }
  ~ScopedOrigin() { OriginStorage() = m_previous; }

  ScopedOrigin(const ScopedOrigin &) = delete;
  ScopedOrigin &operator=(const ScopedOrigin &) = delete;

private:
  Origin m_previous;
};

inline Origin CurrentOrigin() { return OriginStorage(); }

} // namespace selection
