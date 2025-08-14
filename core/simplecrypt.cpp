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
