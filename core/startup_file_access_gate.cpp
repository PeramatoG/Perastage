#include "startup_file_access_gate.h"

namespace StartupFileAccessGate {

std::recursive_mutex &Mutex() {
  static std::recursive_mutex mutex;
  return mutex;
}

} // namespace StartupFileAccessGate

