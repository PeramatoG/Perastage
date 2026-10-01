#pragma once

#include <string>

namespace perastage::capability {

// Serializes the complete capability catalog using discovery schema version 1.
std::string SerializeCatalogJson();

} // namespace perastage::capability
