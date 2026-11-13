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
#include <wx/image.h>
#include <vector>

namespace gui::layoutcache {
// wx images start with the top row. Layout 2D view textures use the same bottom
// row first convention as glReadPixels, and DrawView samples v=1 at the top.
// Other Layout element textures have their own, unchanged sampling convention.
inline bool CopyLayoutViewTexturePixels(const wxImage &image,
                                       std::vector<unsigned char> &pixels) {
  pixels.clear();
  if (!image.IsOk()) return false;
  const int width=image.GetWidth(), height=image.GetHeight();
  const auto *rgb=image.GetData();
  const auto *alpha=image.HasAlpha() ? image.GetAlpha() : nullptr;
  if (!rgb || width<=0 || height<=0) return false;
  pixels.resize(static_cast<size_t>(width)*height*4);
  for (int row=0; row<height; ++row) for (int x=0; x<width; ++x) {
    const size_t source=static_cast<size_t>(height-1-row)*width+x;
    const size_t target=(static_cast<size_t>(row)*width+x)*4;
    pixels[target]=rgb[source*3]; pixels[target+1]=rgb[source*3+1];
    pixels[target+2]=rgb[source*3+2]; pixels[target+3]=alpha ? alpha[source] : 255;
  }
  return true;
}
}
