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
#include "LayoutCollection.h"

namespace layouts {

LayoutCollection::LayoutCollection() { layouts.push_back(DefaultLayout()); }

const std::vector<LayoutDefinition> &LayoutCollection::Items() const {
  return layouts;
}

std::size_t LayoutCollection::Count() const { return layouts.size(); }

bool LayoutCollection::AddLayout(const LayoutDefinition &layout) {
  if (layout.name.empty() || NameExists(layout.name))
    return false;
  layouts.push_back(layout);
  return true;
}

bool LayoutCollection::RenameLayout(const std::string &currentName,
                                    const std::string &newName) {
  if (newName.empty())
    return false;
  if (NameExists(newName, currentName))
    return false;
  for (auto &layout : layouts) {
    if (layout.name == currentName) {
      layout.name = newName;
      return true;
    }
  }
  return false;
}

bool LayoutCollection::RemoveLayout(const std::string &name) {
  if (layouts.size() <= 1)
    return false;
  for (auto it = layouts.begin(); it != layouts.end(); ++it) {
    if (it->name == name) {
      layouts.erase(it);
      return true;
    }
  }
  return false;
}

bool LayoutCollection::SetLayoutOrientation(const std::string &name,
                                            bool landscape) {
  for (auto &layout : layouts) {
    if (layout.name == name) {
      layout.pageSetup.landscape = landscape;
      return true;
    }
  }
  return false;
}

bool LayoutCollection::UpdateLayout2DView(const std::string &name,
                                          const Layout2DViewDefinition &view) {
  for (auto &layout : layouts) {
    if (layout.name == name) {
      bool replaced = false;
      for (auto &entry : layout.view2dViews) {
        if (entry.camera.view == view.camera.view) {
          entry = view;
          replaced = true;
          break;
        }
      }
      if (!replaced)
        layout.view2dViews.push_back(view);
      return true;
    }
  }
  return false;
}

void LayoutCollection::ReplaceAll(std::vector<LayoutDefinition> updated) {
  if (updated.empty()) {
    layouts = {DefaultLayout()};
    return;
  }
  layouts = std::move(updated);
}

LayoutDefinition LayoutCollection::DefaultLayout() {
  LayoutDefinition layout;
  layout.name = "Layout 1";
  layout.pageSetup.pageSize = print::PageSize::A4;
  layout.pageSetup.landscape = false;
  layout.view2dViews.clear();
  return layout;
}

bool LayoutCollection::NameExists(const std::string &name,
                                  const std::string &ignoreName) const {
  for (const auto &layout : layouts) {
    if (layout.name == name && layout.name != ignoreName)
      return true;
  }
  return false;
}

} // namespace layouts
