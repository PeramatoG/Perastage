#pragma once

#include <mutex>

namespace StartupFileAccessGate {

std::recursive_mutex &Mutex();

} // namespace StartupFileAccessGate

