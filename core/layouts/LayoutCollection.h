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

#include <string>
#include <vector>

namespace layouts {

enum class PageSize { A3 = 0, A4 = 1 };

struct LayoutDefinition {
  std::string name;
  PageSize pageSize = PageSize::A4;
  bool landscape = false;
  std::string viewId;
};

class LayoutCollection {
public:
  LayoutCollection();

  const std::vector<LayoutDefinition> &Items() const;
  std::size_t Count() const;

  bool AddLayout(const LayoutDefinition &layout);
  bool RenameLayout(const std::string &currentName,
                    const std::string &newName);
  bool RemoveLayout(const std::string &name);

  void ReplaceAll(std::vector<LayoutDefinition> layouts);

private:
  static LayoutDefinition DefaultLayout();
  bool NameExists(const std::string &name,
                  const std::string &ignoreName = {}) const;

  std::vector<LayoutDefinition> layouts;
};

} // namespace layouts
