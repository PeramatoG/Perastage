/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include <wx/bitmap.h>
#include <wx/colour.h>
#include <wx/size.h>

class GdtfResourceBitmapCache {
public:
  explicit GdtfResourceBitmapCache(std::size_t maxBytes = 16u * 1024u * 1024u);
  void Clear();
  wxBitmap GetOrCreate(const std::string &sourceId, const std::string &entryPath,
                       const std::vector<unsigned char> &bytes, const wxSize &targetSize,
                       const wxColour &placeholderColor);

private:
  struct Entry {
    wxBitmap bitmap;
    std::size_t bytes = 0;
  };
  std::string MakeKey(const std::string &sourceId, const std::string &entryPath,
                      const wxSize &targetSize) const;
  wxBitmap MakePlaceholder(const wxSize &targetSize, const wxColour &color) const;
  void EnforceLimit();

  std::map<std::string, Entry> entries;
  std::size_t maxCacheBytes = 0;
  std::size_t currentBytes = 0;
};
