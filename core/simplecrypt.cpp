#include "simplecrypt.h"
#include <sstream>
#include <iomanip>

namespace {
    constexpr unsigned char KEY = 0x5A;
}

std::string SimpleCrypt::Encode(const std::string& data) {
    std::ostringstream oss;
    for (unsigned char c : data) {
        unsigned char v = c ^ KEY;
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)v;
    }
    return oss.str();
}

std::string SimpleCrypt::Decode(const std::string& data) {
    std::string out;
    out.reserve(data.size() / 2);
    for (size_t i = 0; i + 1 < data.size(); i += 2) {
        unsigned int v = 0;
        std::istringstream iss(data.substr(i,2));
        iss >> std::hex >> v;
        out.push_back(static_cast<char>((unsigned char)v ^ KEY));
    }
    return out;
}
