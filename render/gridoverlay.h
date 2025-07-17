#pragma once

#include <wx/dc.h>
#include <wx/gdicmn.h>
#include "camera.h"

void DrawGridAndAxes(wxDC& dc, const SimpleCamera& cam, const wxSize& size);
