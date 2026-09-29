#pragma once

class wxStyledTextCtrl;

namespace gui::inspection {

// Configures the read-only XML editor, including theme-aware folding markers.
void ConfigureInspectorXmlEditor(wxStyledTextCtrl &editor);

// Recomputes lexer styles and fold levels for the current XML buffer.
void ColouriseInspectorXml(wxStyledTextCtrl &editor);

} // namespace gui::inspection
