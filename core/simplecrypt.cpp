/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "simplecrypt.h"
#include <sstream>
#include <iomanip>
#include <cctype>

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


// Strictly decodes legacy XOR/hex data for credential migration.
SimpleCrypt::DecodeResult SimpleCrypt::DecodeStrict(const std::string& data) {
    DecodeResult result;
    if (data.empty()) {
        result.error = "Encoded value is empty";
        return result;
    }
    if ((data.size() % 2) != 0) {
        result.error = "Encoded value has odd length";
        return result;
    }
    std::string out;
    out.reserve(data.size() / 2);
    for (size_t i = 0; i < data.size(); i += 2) {
        const unsigned char hi = static_cast<unsigned char>(data[i]);
        const unsigned char lo = static_cast<unsigned char>(data[i + 1]);
        if (!std::isxdigit(hi) || !std::isxdigit(lo)) {
            result.error = "Encoded value contains non-hex characters";
            return result;
        }
        unsigned int v = 0;
        std::istringstream iss(data.substr(i, 2));
        iss >> std::hex >> v;
        if (iss.fail()) {
            result.error = "Encoded value could not be parsed";
            return result;
        }
        out.push_back(static_cast<char>((static_cast<unsigned char>(v)) ^ KEY));
    }
    result.success = true;
    result.value = std::move(out);
    return result;
}
