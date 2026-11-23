#pragma once

#include <wx/string.h>

namespace layouttext {
// Retain a recovery copy of source that the native editor cannot round-trip.
wxString LegacyRichTextTail(const wxString &original, const wxString &nativeRoundTrip);
}
