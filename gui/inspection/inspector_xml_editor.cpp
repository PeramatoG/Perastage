#include "inspection/inspector_xml_editor.h"

#include <wx/settings.h>
#include <wx/stc/stc.h>

namespace gui::inspection {
namespace {

constexpr int kXmlLineNumberMargin = 0;
constexpr int kXmlFoldMargin = 1;

// Defines one folder marker using colours that contrast with the editor theme.
void DefineFoldMarker(wxStyledTextCtrl &editor, int marker, int symbol,
                      const wxColour &foreground,
                      const wxColour &background) {
  editor.MarkerDefine(marker, symbol, foreground, background);
}

} // namespace

// Configures the read-only XML editor, including theme-aware folding markers.
void ConfigureInspectorXmlEditor(wxStyledTextCtrl &editor) {
  const wxColour foreground =
      wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
  const wxColour background =
      wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
  const bool dark = background.GetLuminance() < 0.5;
  editor.StyleSetForeground(wxSTC_STYLE_DEFAULT, foreground);
  editor.StyleSetBackground(wxSTC_STYLE_DEFAULT, background);
  editor.StyleClearAll();
  editor.SetLexer(wxSTC_LEX_XML);
  editor.StyleSetForeground(wxSTC_H_TAG,
                            dark ? wxColour(110, 190, 255)
                                 : wxColour(0, 96, 160));
  editor.StyleSetForeground(wxSTC_H_ATTRIBUTE,
                            dark ? wxColour(255, 190, 100)
                                 : wxColour(128, 64, 0));
  const wxColour stringColour =
      dark ? wxColour(120, 220, 150) : wxColour(0, 112, 48);
  editor.StyleSetForeground(wxSTC_H_DOUBLESTRING, stringColour);
  editor.StyleSetForeground(wxSTC_H_SINGLESTRING, stringColour);
  editor.StyleSetForeground(wxSTC_H_COMMENT,
                            dark ? wxColour(180, 180, 180)
                                 : wxColour(96, 96, 96));
  editor.SetProperty("fold", "1");
  editor.SetProperty("fold.html", "1");
  editor.SetProperty("fold.compact", "1");
  editor.SetProperty("fold.html.preprocessor", "1");
  editor.SetFoldFlags(wxSTC_FOLDFLAG_LINEBEFORE_CONTRACTED |
                      wxSTC_FOLDFLAG_LINEAFTER_CONTRACTED);
  editor.SetMarginType(kXmlLineNumberMargin, wxSTC_MARGIN_NUMBER);
  editor.SetMarginWidth(kXmlLineNumberMargin, 0);
  editor.SetMarginSensitive(kXmlLineNumberMargin, false);
  editor.StyleSetForeground(wxSTC_STYLE_LINENUMBER, foreground);
  editor.StyleSetBackground(wxSTC_STYLE_LINENUMBER, background);
  editor.SetMarginType(kXmlFoldMargin, wxSTC_MARGIN_SYMBOL);
  editor.SetMarginMask(kXmlFoldMargin, wxSTC_MASK_FOLDERS);
  editor.SetMarginWidth(kXmlFoldMargin, 16);
  editor.SetMarginSensitive(kXmlFoldMargin, true);
  DefineFoldMarker(editor, wxSTC_MARKNUM_FOLDER, wxSTC_MARK_BOXPLUS,
                   foreground, background);
  DefineFoldMarker(editor, wxSTC_MARKNUM_FOLDEROPEN, wxSTC_MARK_BOXMINUS,
                   foreground, background);
  DefineFoldMarker(editor, wxSTC_MARKNUM_FOLDERSUB, wxSTC_MARK_VLINE,
                   foreground, background);
  DefineFoldMarker(editor, wxSTC_MARKNUM_FOLDEREND,
                   wxSTC_MARK_BOXPLUSCONNECTED, foreground, background);
  DefineFoldMarker(editor, wxSTC_MARKNUM_FOLDEROPENMID,
                   wxSTC_MARK_BOXMINUSCONNECTED, foreground, background);
  DefineFoldMarker(editor, wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_TCORNER,
                   foreground, background);
  DefineFoldMarker(editor, wxSTC_MARKNUM_FOLDERTAIL, wxSTC_MARK_LCORNER,
                   foreground, background);
  editor.SetReadOnly(true);
}

// Recomputes lexer styles and fold levels for the current XML buffer.
void ColouriseInspectorXml(wxStyledTextCtrl &editor) {
  editor.Colourise(0, -1);
}

} // namespace gui::inspection
