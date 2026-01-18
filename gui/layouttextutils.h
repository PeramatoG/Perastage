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

#include <wx/gdicmn.h>
#include <wx/image.h>
#include <wx/richtext/richtextbuffer.h>

#include "layouts/LayoutCollection.h"
#include "viewer2dpdfexporter.h"

namespace layouttext {
bool LoadRichTextBufferFromString(wxRichTextBuffer &buffer,
                                  const wxString &content);
wxString SaveRichTextBufferToString(wxRichTextBuffer &buffer);

LayoutTextExportData BuildLayoutTextExportData(
    const layouts::LayoutTextDefinition &text, double scaleX, double scaleY);

wxImage RenderTextImage(const layouts::LayoutTextDefinition &text,
                        const wxSize &renderSize, const wxSize &logicalSize,
                        double renderScale);
} // namespace layouttext
