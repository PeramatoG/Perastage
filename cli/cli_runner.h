/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

namespace perastage::cli {

int Run(std::span<const std::string_view> args,
        std::ostream& out,
        std::ostream& err);

} // namespace perastage::cli
