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
#include "LayoutDocumentEdit.h"
#include "LayoutManager.h"

namespace layouts {
bool LayoutManager::EditElements(const std::string &name,
    const std::function<LayoutDefinition(const LayoutDefinition &)> &edit,
    const std::string &description, std::string *error) {
  if (error) error->clear();
  const bool result = ApplyMutation(description, [&](LayoutCollection &candidate) {
    try {
      for (auto &page : candidate.Items()) {
        if (page.name != name) continue;
        page = edit(page);
        return true;
      }
      throw std::invalid_argument("Layout page does not exist.");
    } catch (const std::exception &e) {
      if (error) *error = e.what();
      return false;
    }
  });
  if (!result && error && error->empty()) *error = "Layout mutation could not be published.";
  return result;
}
bool LayoutManager::AlignElements(const std::string &name,
    const std::vector<LayoutElementRef> &refs, alignment::Reference reference,
    alignment::Operation operation, bool origins, std::string *error) {
  return EditElements(name, [&](const auto &page) {
    return layouts::AlignElements(page, refs, reference, operation, origins);
  }, "align layout elements", error);
}
bool LayoutManager::TranslateElements(const std::string &name,
    const std::vector<LayoutElementRef> &refs, double dx, double dy, std::string *error) {
  return EditElements(name, [&](const auto &page) {
    return layouts::TranslateElements(page, refs, dx, dy);
  }, "move layout elements", error);
}
bool LayoutManager::DeleteElements(const std::string &name,
    const std::vector<LayoutElementRef> &refs, std::string *error) {
  return EditElements(name, [&](const auto &page) {
    return layouts::DeleteElements(page, refs);
  }, "delete layout elements", error);
}
} // namespace layouts
