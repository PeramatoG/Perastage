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

#include <wx/dataview.h>

namespace ColumnUtils {
namespace detail {
template <typename Column>
auto SetEllipsizeModeIfSupported(Column *column, wxEllipsizeMode mode)
    -> decltype(column->SetEllipsizeMode(mode), void()) {
  if (!column)
    return;
  column->SetEllipsizeMode(mode);
}

inline void SetEllipsizeModeIfSupported(...) {}
} // namespace detail

inline void EnforceMinColumnWidth(wxDataViewListCtrl *table,
                                  int minWidth = 50) {
  if (!table)
    return;

  const unsigned int count = table->GetColumnCount();
  for (unsigned int i = 0; i < count; ++i) {
    if (auto *col = table->GetColumn(i)) {
      col->SetMinWidth(minWidth);
      detail::SetEllipsizeModeIfSupported(col, wxELLIPSIZE_NONE);
    }
  }
}
} // namespace ColumnUtils
