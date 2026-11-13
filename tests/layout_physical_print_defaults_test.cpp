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
#include "ui_feature_flags.h"
#include "Viewer2DPrintSettings.h"
#include "PhysicalViewport.h"
#include <iostream>

// Uses explicit failures so the Release-profile assertions remain active.
int main() {
  print::Viewer2DPrintSettings legacy;
  legacy.pageSize=print::PageSize::A3;
  ui::ApplyBuildDefaultsToViewer2DPrintSettings(legacy);
  const auto expectedLegacy=PERASTAGE_EXPECT_RELEASE ? print::PageSize::A4 : print::PageSize::A3;
  if (legacy.pageSize!=expectedLegacy) { std::cerr<<"Legacy print defaults changed\n"; return 1; }
  for (auto paper : {print::PageSize::A3, print::PageSize::A4}) {
    print::PageSetup document; document.pageSize=paper;
    print::Viewer2DPrintSettings fixed;
    fixed.pageSize=paper;
    ui::ApplyBuildDefaultsToViewer2DPrintSettings(fixed,document);
    try {
      layouts::ValidatePhysicalOutput(true,fixed.PageWidthPt()/document.PageWidthPt(),
          fixed.PageHeightPt()/document.PageHeightPt());
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
  }
  print::PageSetup document; document.pageSize=print::PageSize::A3;
  print::Viewer2DPrintSettings mismatch; mismatch.pageSize=print::PageSize::A4;
  ui::ApplyBuildDefaultsToViewer2DPrintSettings(mismatch,document);
  const auto expected=PERASTAGE_EXPECT_RELEASE ? print::PageSize::A3 : print::PageSize::A4;
  if (mismatch.pageSize!=expected) { std::cerr<<"Explicit Debug paper selection was overridden\n"; return 1; }
}
