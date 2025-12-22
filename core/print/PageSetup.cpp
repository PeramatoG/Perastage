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
#include "PageSetup.h"

namespace {
constexpr double kMmToPoints = 72.0 / 25.4;
} // namespace

namespace print {

std::pair<double, double> PageSetup::BasePageSizeMm() const {
  switch (pageSize) {
  case PageSize::A4:
    return {210.0, 297.0};
  case PageSize::A3:
  default:
    return {297.0, 420.0};
  }
}

double PageSetup::PageWidthPt() const {
  auto [portraitW, portraitH] = BasePageSizeMm();
  const double mm = landscape ? portraitH : portraitW;
  return mm * kMmToPoints;
}

double PageSetup::PageHeightPt() const {
  auto [portraitW, portraitH] = BasePageSizeMm();
  const double mm = landscape ? portraitW : portraitH;
  return mm * kMmToPoints;
}

} // namespace print
