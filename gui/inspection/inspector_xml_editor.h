#pragma once

class wxStyledTextCtrl;

namespace gui::inspection {

enum class InspectorSourceSyntax;

// Configures the read-only XML editor, including theme-aware folding markers.
void ConfigureInspectorXmlEditor(wxStyledTextCtrl &editor);

// Recomputes lexer styles and fold levels for the current XML buffer.
void ColouriseInspectorXml(wxStyledTextCtrl &editor);

// Applies XML or plain-text presentation without changing editor ownership.
void SetInspectorSourceSyntax(wxStyledTextCtrl &editor,
                              InspectorSourceSyntax syntax);

// Reports whether folding actions are meaningful for the current source mode.
bool InspectorSourceSupportsFolding(InspectorSourceSyntax syntax);

} // namespace gui::inspection
