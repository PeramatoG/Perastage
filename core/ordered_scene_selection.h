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
#include "scene_grouping.h"
#include <algorithm>

// Runtime chronology only; never serialized to MVR or used to dirty the project.
class OrderedSceneSelection {
public:
  using Identity = scene_grouping::SceneTransformTarget;
  const std::vector<Identity> &Items() const { return items_; }
  // Existing identities keep order. A bulk addition uses ascending UUID order.
  void Reconcile(MvrNodeType type, const std::vector<std::string> &membership) {
    std::erase_if(items_, [&](const Identity &item) {
      return item.type == type &&
        std::find(membership.begin(), membership.end(), item.uuid) == membership.end();
    });
    auto incoming = membership;
    std::sort(incoming.begin(), incoming.end());
    for (const auto &uuid : incoming) {
      if (!uuid.empty() && std::none_of(items_.begin(), items_.end(), [&](const Identity &i) {
            return i.type == type && i.uuid == uuid;
          })) items_.push_back({type, uuid});
    }
  }
  // Explicit replacement honors caller order and ignores duplicate identities.
  void Replace(const std::vector<Identity> &ordered) {
    items_.clear();
    for (const auto &identity : ordered)
      if (!identity.uuid.empty() && std::none_of(items_.begin(), items_.end(), [&](const Identity &i) {
            return i.type == identity.type && i.uuid == identity.uuid;
          })) items_.push_back(identity);
  }
  void Clear() { items_.clear(); }
private:
  std::vector<Identity> items_;
};
