#pragma once

#include <mutex>

namespace StartupFileAccessGate {

inline std::recursive_mutex &Mutex() {
  static std::recursive_mutex mutex;
  return mutex;
}

} // namespace StartupFileAccessGate
