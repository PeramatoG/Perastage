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

#include <wx/glcanvas.h>
#include <vector>
#include <string>
#include "viewer3dcamera.h"
#include "gdtfloader.h"

// Simple 3D preview panel for a single fixture
class FixturePreviewPanel : public wxGLCanvas {
public:
    explicit FixturePreviewPanel(wxWindow* parent);
    ~FixturePreviewPanel();

    // Loads fixture model from a GDTF file. When loading fails a
    // simple cube will be displayed instead.
    void LoadFixture(const std::string& gdtfPath);

private:
    void OnPaint(wxPaintEvent& evt);
    void OnResize(wxSizeEvent& evt);
    void OnMouseDown(wxMouseEvent& evt);
    void OnMouseUp(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
    void OnMouseWheel(wxMouseEvent& evt);
    void OnCaptureLost(wxMouseCaptureLostEvent& evt);

    void InitGL();
    void Render();

    wxGLContext* m_glContext = nullptr;
    bool m_glInitialized = false;

    Viewer3DCamera m_camera;
    std::vector<GdtfObject> m_objects;
    bool m_hasModel = false;
    float m_bbMin[3];
    float m_bbMax[3];

    bool m_dragging = false;
    wxPoint m_lastMousePos;

    wxDECLARE_EVENT_TABLE();
};

