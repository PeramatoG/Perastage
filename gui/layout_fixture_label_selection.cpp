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
#include "layout_fixture_label_selection.h"
#include "viewer2dpanel.h"
#include "configmanager.h"
#include <algorithm>
#include <memory>

// Local label targets never publish scene selection, transforms, or history.
void InstallLayoutFixtureLabelSelection(Viewer2DPanel &panel, const ConfigManager *cfg) {
  panel.SetLayoutFixtureLabelSelection(cfg ? cfg->GetSelectedFixtures() : std::vector<std::string>{});
  auto press = std::make_shared<std::optional<wxPoint>>();
  panel.Bind(wxEVT_LEFT_DOWN, [press](wxMouseEvent &event) {
    *press = event.GetPosition();
    event.Skip();
  });
  panel.Bind(wxEVT_LEFT_UP, [&panel, press](wxMouseEvent &event) {
    if (*press) {
      const auto delta = event.GetPosition() - **press;
      if (delta.x*delta.x + delta.y*delta.y <= 9) {
        auto selected = panel.layoutFixtureLabelSelection.value_or(std::vector<std::string>{});
        const auto fixture = panel.PickFixtureForLayoutLabels(event.GetPosition());
        if (!event.ShiftDown() && !event.ControlDown()) selected.clear();
        if (fixture) {
          const auto it = std::find(selected.begin(), selected.end(), *fixture);
          if (it == selected.end()) selected.push_back(*fixture);
          else selected.erase(it);
        }
        panel.SetLayoutFixtureLabelSelection(std::move(selected));
      }
      press->reset();
    }
    // The ordinary disabled-selection handler still completes camera panning.
    event.Skip();
  });
}
