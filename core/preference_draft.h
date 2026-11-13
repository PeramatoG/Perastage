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
#include <cassert>
#include <functional>
#include <string_view>
#include <utility>

// Suppresses dirty publication only for explicitly owned temporary preferences.
// Scene/history mutations and unrelated keys remain ordinary project edits.
class ScopedPreferenceDraft {
public:
  ScopedPreferenceDraft(const void *owner, std::function<bool(std::string_view)> ownsKey)
      : owner_(owner), ownsKey_(std::move(ownsKey)), previous_(current_) { current_ = this; }
  ~ScopedPreferenceDraft() { assert(current_ == this); current_ = previous_; }
  ScopedPreferenceDraft(const ScopedPreferenceDraft &) = delete;
  ScopedPreferenceDraft &operator=(const ScopedPreferenceDraft &) = delete;

  static bool Owns(const void *owner, std::string_view key) {
    for (auto *scope = current_; scope; scope = scope->previous_)
      if (scope->owner_ == owner && scope->ownsKey_(key)) return true;
    return false;
  }
private:
  const void *owner_;
  std::function<bool(std::string_view)> ownsKey_;
  ScopedPreferenceDraft *previous_;
  inline static thread_local ScopedPreferenceDraft *current_ = nullptr;
};
