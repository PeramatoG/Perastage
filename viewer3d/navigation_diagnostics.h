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
/*
 * File: navigation_diagnostics.h
 * Author: Luisma Peramato
 * License: GNU General Public License v3.0
 * Description: Debug-only diagnostics for high-rate 3D viewer navigation.
 */

#pragma once

#include <wx/log.h>

namespace viewer3d::diagnostics {

// Writes a debug-only trace line for a 3D navigation or rendering operation.
inline void Log(const char* message)
{
#ifndef NDEBUG
    wxLogTrace("viewer3d_navigation", "%s", message);
#else
    (void)message;
#endif
}

// Writes a debug-only formatted trace line for a 3D navigation or rendering operation.
template <typename... Args>
inline void Logf(const char* format, Args... args)
{
#ifndef NDEBUG
    wxLogTrace("viewer3d_navigation", format, args...);
#else
    (void)format;
    ((void)args, ...);
#endif
}

} // namespace viewer3d::diagnostics
