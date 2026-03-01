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
#pragma once

namespace app {

inline constexpr const char *kName = "Perastage";

#ifndef PERASTAGE_APP_VERSION
#define PERASTAGE_APP_VERSION "0.1.0"
#endif

#ifndef PERASTAGE_APP_VERSION_DISPLAY
#define PERASTAGE_APP_VERSION_DISPLAY "beta 0.1.0"
#endif

inline constexpr const char *kVersion = PERASTAGE_APP_VERSION;
inline constexpr const char *kVersionDisplay = PERASTAGE_APP_VERSION_DISPLAY;

} // namespace app
