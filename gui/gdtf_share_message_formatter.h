#pragma once

#include "gdtfnet.h"

#include <wx/string.h>

enum class GdtfShareGuiOperation { Login, Download };

// Formats a complete localized GUI message from a structured GDTF Share result.
wxString FormatLocalizedGdtfShareUserMessage(
    const GdtfShareResult &result, GdtfShareGuiOperation operation);
