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
#include "viewer2dpanel.h"
#include "fixture_label_presentation.h"
#include "LayoutManager.h"
void Viewer2DPanel::ApplyFixtureLabelPresentation(const layouts::FixtureLabelPresentation &settings,
                                                 const ConfigManager &cfg) {
  auto context = viewer2d::BuildLabelPresentationContext(cfg, settings,
      layouts::LayoutManager::Get().GetLayouts().labelProfiles);
  labelPresentation = settings;
  SetFixtureLabelPresentationContext(std::move(context));
}
