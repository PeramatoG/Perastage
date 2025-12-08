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

#include "uuidutils.h"

#include <random>

std::string GenerateUuid() {
  static std::mt19937_64 rng{std::random_device{}()};
  static std::uniform_int_distribution<int> dist(0, 15);
  const char *v = "0123456789abcdef";
  int groups[] = {8, 4, 4, 4, 12};
  std::string out;
  for (int g = 0; g < 5; ++g) {
    if (g)
      out.push_back('-');
    for (int i = 0; i < groups[g]; ++i)
      out.push_back(v[dist(rng)]);
  }
  return out;
}
