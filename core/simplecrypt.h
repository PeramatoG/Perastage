#pragma once
#include <string>

namespace SimpleCrypt {
    std::string Encode(const std::string& data);
    std::string Decode(const std::string& data);
}
