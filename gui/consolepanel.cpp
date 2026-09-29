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
#include "consolepanel.h"
#include "command/command_transform_text_adapter.h"
#include "command/command_text_parser.h"
#include "configmanager.h"
#include "fixturetablepanel.h"
#include "guiconfigservices.h"
#include "hoisttablepanel.h"
#include "mainwindow.h"
#include "project_fixture_identity.h"
#include "sceneobjecttablepanel.h"
#include "selection_movement_settings.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <optional>
#include <sstream>
#include <vector>
#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/stdpaths.h>

namespace {

class ConsoleProjectMutationHost final
    : public perastage::command::ProjectMutationHost {
public:
  // Creates a Console mutation publisher backed by the active project.
  explicit ConsoleProjectMutationHost(ConfigManager &config) : config_(config) {}

  // Publishes the exact pre-transform scene and selection as one Undo entry.
  perastage::command::MutationPublication CommitMutation(
      const MvrScene &sceneBefore,
      const scene_grouping::ObjectSelection &selectionBefore,
      const std::string &undoLabel) override {
    SelectionState selection;
    selection.SetSelectedFixtures(selectionBefore.fixtures);
    selection.SetSelectedTrusses(selectionBefore.trusses);
    selection.SetSelectedSupports(selectionBefore.supports);
    selection.SetSelectedSceneObjects(selectionBefore.sceneObjects);
    config_.PushUndoSnapshot(
        sceneBefore, selection,
        config_.GetValue(project_identity::kFixtureLabelOverridesConfigKey),
        undoLabel);
    return {true, config_.IsDirty()};
  }

private:
  ConfigManager &config_;
};

enum class ConsoleMessageKind {
  Default,
  Error,
  Warning,
  Command,
  Info,
};

// Returns the stable Console prefix for a structured diagnostic severity.
wxString
ConsoleDiagnosticPrefix(perastage::command::DiagnosticSeverity severity) {
  switch (severity) {
  case perastage::command::DiagnosticSeverity::Warning:
    return "[WARNING] ";
  case perastage::command::DiagnosticSeverity::Information:
    return "[INFO] ";
  case perastage::command::DiagnosticSeverity::Error:
  default:
    return "[ERROR] ";
  }
}

// Formats one parser diagnostic for the technical Console presentation.
wxString
FormatParseDiagnostic(const perastage::command::Diagnostic &diagnostic) {
  const wxString prefix = ConsoleDiagnosticPrefix(diagnostic.severity);
  if (diagnostic.code == "command_text.unknown_command")
    return prefix + "Syntax error";
  return prefix + wxString::FromUTF8(diagnostic.message);
}

// Formats one semantic command diagnostic for Console presentation.
wxString FormatCommandDiagnostic(
    const perastage::command::Diagnostic &diagnostic) {
  return ConsoleDiagnosticPrefix(diagnostic.severity) +
         wxString::FromUTF8(diagnostic.message);
}

ConsoleMessageKind DetectMessageKind(const wxString &message) {
  if (!message.StartsWith("["))
    return ConsoleMessageKind::Default;

  const int closing = message.Find(']');
  if (closing == wxNOT_FOUND || closing <= 1)
    return ConsoleMessageKind::Default;

  const wxString tag = message.Mid(1, closing - 1).Upper();
  if (tag == "ERROR")
    return ConsoleMessageKind::Error;
  if (tag == "WARNING")
    return ConsoleMessageKind::Warning;
  if (tag == "CMD")
    return ConsoleMessageKind::Command;
  if (tag == "INFO")
    return ConsoleMessageKind::Info;

  return ConsoleMessageKind::Default;
}

wxColour ColorForMessageKind(ConsoleMessageKind kind) {
  switch (kind) {
  case ConsoleMessageKind::Error:
    return wxColour(255, 80, 80);
  case ConsoleMessageKind::Warning:
    return wxColour(255, 180, 60);
  case ConsoleMessageKind::Command:
    return wxColour(235, 235, 235);
  case ConsoleMessageKind::Info:
  case ConsoleMessageKind::Default:
  default:
    return wxColour(0, 255, 0);
  }
}

void AppendStyledConsoleLine(wxTextCtrl *textCtrl, const wxString &line) {
  if (!textCtrl)
    return;

  const wxColour messageColour = ColorForMessageKind(DetectMessageKind(line));
  textCtrl->SetDefaultStyle(wxTextAttr(messageColour));
  textCtrl->AppendText(line + "\n");
}

wxString ReadUtf8File(const wxString &path) {
  std::ifstream in(path.ToStdString());
  if (!in)
    return {};
  std::stringstream buffer;
  buffer << in.rdbuf();
  return wxString::FromUTF8(buffer.str());
}

wxString ExtractConsoleSection(const wxString &markdown,
                               const wxString &header) {
  const wxString startToken = "## " + header;
  const int start = markdown.Find(startToken);
  if (start == wxNOT_FOUND)
    return {};

  const int sectionStart = start + static_cast<int>(startToken.length());
  wxString rest = markdown.Mid(sectionStart);
  const int nextHeader = rest.Find("\n## ");
  if (nextHeader != wxNOT_FOUND)
    rest = rest.Left(nextHeader);

  wxArrayString lines = wxSplit(rest, '\n', '\0');
  wxString result;
  for (const wxString &line : lines) {
    wxString clean = line;
    clean.Trim(true).Trim(false);
    if (clean.IsEmpty()) {
      result += "\n";
      continue;
    }

    if (clean.StartsWith("| ---"))
      continue;
    if (clean.StartsWith("### ")) {
      result += clean.Mid(4) + "\n";
      continue;
    }
    if (clean.StartsWith("## ")) {
      result += clean.Mid(3) + "\n";
      continue;
    }
    if (clean.StartsWith("|")) {
      wxString tableLine = clean;
      tableLine.Replace("|", " ");
      tableLine.Trim(true).Trim(false);
      result += tableLine + "\n";
      continue;
    }
    result += clean + "\n";
  }

  return result.Trim();
}

// Loads stable English Console help with a built-in fallback.
wxString BuildConsoleHelpContent() {
  wxFileName helpPath(wxStandardPaths::Get().GetExecutablePath());
  helpPath.SetFullName("help.md");
  const wxString markdown = ReadUtf8File(helpPath.GetFullPath());

  const wxString preferredHeader = "Console Commands (complete)";
  const wxString fallbackHeader = "Comandos de consola (completo)";

  wxString section = ExtractConsoleSection(markdown, preferredHeader);
  if (section.IsEmpty())
    section = ExtractConsoleSection(markdown, fallbackHeader);
  if (!section.IsEmpty())
    return section;

  wxString help = wxString("Console commands:");
  help += "\n- clear\n- f ...\n- t ...\n";
  help += "- pos x|y|z <values>\n- pos <x>,<y>,<z>\n";
  help += "- x|y|z <values>\n";
  help += "- rot x|y|z <values> [--group|--g] [pivotX,pivotY,pivotZ]\n";
  help += wxString("Examples:");
  help += "\n- f 1-5\n- pos x 1 4\n- pos x ++1 --local\n- rot z --10\n";
  help += "- rot y ++45 --g --local -2.5,0,0";
  return help;
}

} // namespace

ConsolePanel::ConsolePanel(wxWindow *parent) : wxPanel(parent, wxID_ANY) {
  wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
  m_textCtrl =
      new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                     wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
  m_textCtrl->SetBackgroundColour(*wxBLACK);
  m_textCtrl->SetForegroundColour(wxColour(0, 255, 0));
  wxFont font(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL,
              wxFONTWEIGHT_NORMAL);
  m_textCtrl->SetFont(font);
  m_inputCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition,
                               wxDefaultSize, wxTE_PROCESS_ENTER);
  m_inputCtrl->SetFont(font);
  m_inputCtrl->Bind(wxEVT_TEXT_ENTER, &ConsolePanel::OnCommandEnter, this);
  m_inputCtrl->Bind(wxEVT_SET_FOCUS, &ConsolePanel::OnInputFocus, this);
  m_inputCtrl->Bind(wxEVT_KILL_FOCUS, &ConsolePanel::OnInputKillFocus, this);
  m_inputCtrl->Bind(wxEVT_KEY_DOWN, &ConsolePanel::OnInputKeyDown, this);
  m_inputCtrl->SetValue(">>> ");
  m_inputCtrl->SetInsertionPointEnd();
  m_helpButton = new wxButton(this, wxID_ANY, "?", wxDefaultPosition,
                              wxSize(24, 24), wxBU_EXACTFIT);
  m_helpButton->SetToolTip(_("Show available console commands and examples."));
  m_helpButton->Bind(wxEVT_BUTTON, &ConsolePanel::OnHelpButton, this);

  wxBoxSizer *inputSizer = new wxBoxSizer(wxHORIZONTAL);
  inputSizer->Add(m_inputCtrl, 1, wxEXPAND);
  inputSizer->Add(m_helpButton, 0, wxLEFT, 4);

  const wxEventTypeTag<wxScrollWinEvent> scrollEvents[] = {
      wxEVT_SCROLLWIN_TOP,        wxEVT_SCROLLWIN_BOTTOM,
      wxEVT_SCROLLWIN_LINEUP,     wxEVT_SCROLLWIN_LINEDOWN,
      wxEVT_SCROLLWIN_PAGEUP,     wxEVT_SCROLLWIN_PAGEDOWN,
      wxEVT_SCROLLWIN_THUMBTRACK, wxEVT_SCROLLWIN_THUMBRELEASE};
  for (const auto &evt : scrollEvents)
    m_textCtrl->Bind(evt, &ConsolePanel::OnScroll, this);
  sizer->Add(m_textCtrl, 1, wxEXPAND | wxALL, 5);
  sizer->Add(inputSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  SetSizer(sizer);
}

void ConsolePanel::OnHelpButton(wxCommandEvent &) {
  wxDialog helpDialog(this, wxID_ANY, _("Console commands"), wxDefaultPosition,
                      wxSize(620, 420),
                      wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  auto *dialogSizer = new wxBoxSizer(wxVERTICAL);
  auto *helpText = new wxTextCtrl(
      &helpDialog, wxID_ANY, BuildConsoleHelpContent(), wxDefaultPosition,
      wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
  helpText->SetBackgroundColour(*wxBLACK);
  helpText->SetForegroundColour(wxColour(230, 230, 230));
  helpText->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL,
                           wxFONTWEIGHT_NORMAL));
  dialogSizer->Add(helpText, 1, wxEXPAND | wxALL, 8);
  dialogSizer->Add(helpDialog.CreateButtonSizer(wxOK), 0,
                   wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);
  helpDialog.SetSizerAndFit(dialogSizer);
  helpDialog.SetSize(620, 420);
  helpDialog.ShowModal();
}

void ConsolePanel::AppendMessage(const wxString &msg) {
  if (!m_textCtrl)
    return;

  constexpr size_t kMaxConsoleMessageLength = 8 * 1024;
  const wxString suffix = "... (truncated)";
  wxString safeMsg = msg;
  if (safeMsg.length() > kMaxConsoleMessageLength) {
    size_t keepLength = kMaxConsoleMessageLength > suffix.length()
                            ? kMaxConsoleMessageLength - suffix.length()
                            : 0;
    safeMsg = safeMsg.Left(keepLength) + suffix;
  }

  if (safeMsg == m_lastMessage) {
    m_repeatCount++;
    wxString combined = safeMsg + " (repeated " +
                        wxString::Format("%zu", m_repeatCount) + " times)";
    long endPos = m_textCtrl->GetLastPosition();
    if (m_lastLineStart < endPos)
      m_textCtrl->Remove(m_lastLineStart, endPos);
    AppendStyledConsoleLine(m_textCtrl, combined);
  } else {
    m_lastMessage = safeMsg;
    m_repeatCount = 1;
    m_lastLineStart = m_textCtrl->GetLastPosition();
    AppendStyledConsoleLine(m_textCtrl, safeMsg);
  }
  if (m_autoScroll)
    m_textCtrl->ShowPosition(m_textCtrl->GetLastPosition());
}

static ConsolePanel *s_instance = nullptr;

ConsolePanel *ConsolePanel::Instance() { return s_instance; }

void ConsolePanel::SetInstance(ConsolePanel *panel) { s_instance = panel; }

bool ConsolePanel::IsInputWidgetOrChild(const wxWindow *window) const {
  if (!window || !m_inputCtrl)
    return false;

  const wxWindow *current = window;
  while (current) {
    if (current == m_inputCtrl)
      return true;
    current = current->GetParent();
  }
  return false;
}

bool ConsolePanel::InputHasTypedContent() const {
  if (!m_inputCtrl)
    return false;

  wxString value = m_inputCtrl->GetValue();
  if (value.StartsWith(">>> "))
    value = value.Mid(4);
  value.Trim(true).Trim(false);
  return !value.IsEmpty();
}

void ConsolePanel::FocusInputWithOptionalPrefill(const wxString &text) {
  if (!m_inputCtrl)
    return;

  if (!text.IsEmpty() && !InputHasTypedContent())
    m_inputCtrl->SetValue(">>> " + text);

  m_inputCtrl->SetFocus();
  m_inputCtrl->SetInsertionPointEnd();
}

void ConsolePanel::OnScroll(wxScrollWinEvent &event) {
  if (!m_textCtrl) {
    event.Skip();
    return;
  }
  int maxPos = m_textCtrl->GetScrollRange(wxVERTICAL) -
               m_textCtrl->GetScrollThumb(wxVERTICAL);
  int pos = event.GetPosition();
  m_autoScroll = (pos >= maxPos);
  event.Skip();
}

// --- Input handling ---

void ConsolePanel::OnCommandEnter(wxCommandEvent &event) {
  wxString cmd = m_inputCtrl ? m_inputCtrl->GetValue() : wxString();
  if (cmd.StartsWith(">>> "))
    cmd = cmd.Mid(4);
  if (!cmd.IsEmpty()) {
    m_history.push_back(cmd);
    m_historyIndex = m_history.size();
  }
  if (m_inputCtrl) {
    m_inputCtrl->SetValue(">>> ");
    m_inputCtrl->SetInsertionPointEnd();
  }
  ProcessCommand(cmd);
}

void ConsolePanel::OnInputFocus(wxFocusEvent &event) {
  if (MainWindow::Instance())
    MainWindow::Instance()->EnableShortcuts(false);
  if (m_inputCtrl)
    m_inputCtrl->SetInsertionPointEnd();
  event.Skip();
}

void ConsolePanel::OnInputKillFocus(wxFocusEvent &event) {
  if (MainWindow::Instance())
    MainWindow::Instance()->EnableShortcuts(true);
  event.Skip();
}

void ConsolePanel::OnInputKeyDown(wxKeyEvent &event) {
  int code = event.GetKeyCode();
  long pos = m_inputCtrl ? m_inputCtrl->GetInsertionPoint() : 0;
  if (code == WXK_ESCAPE) {
    if (MainWindow::Instance())
      MainWindow::Instance()->EnableShortcuts(true);
    m_inputCtrl->SetValue(">>> ");
    m_inputCtrl->SetInsertionPointEnd();
    if (m_textCtrl)
      m_textCtrl->SetFocus();
    return;
  }
  if ((code == WXK_BACK || code == WXK_LEFT) && pos <= 4) {
    m_inputCtrl->SetInsertionPoint(4);
    return;
  }
  if (code == WXK_HOME) {
    m_inputCtrl->SetInsertionPoint(4);
    return;
  }
  if (code == WXK_UP) {
    if (!m_history.empty() && m_historyIndex > 0) {
      m_historyIndex--;
      m_inputCtrl->SetValue(">>> " + m_history[m_historyIndex]);
      m_inputCtrl->SetInsertionPointEnd();
    }
    return;
  }
  if (code == WXK_DOWN) {
    if (m_historyIndex + 1 < m_history.size()) {
      m_historyIndex++;
      m_inputCtrl->SetValue(">>> " + m_history[m_historyIndex]);
    } else {
      m_historyIndex = m_history.size();
      m_inputCtrl->SetValue(">>> ");
    }
    m_inputCtrl->SetInsertionPointEnd();
    return;
  }
  event.Skip();
}

// Parses and applies command-bar actions to the current scene selection.
void ConsolePanel::ProcessCommand(const wxString &cmdWx) {
  std::string cmd = std::string(cmdWx.ToUTF8());
  if (cmd.find_first_not_of(" \t\n\r") == std::string::npos)
    return;

  AppendMessage("[CMD] " + cmdWx);

  try {
    ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
    const auto interactiveTransformPolicy =
        selection_movement_settings::LoadInteractiveTransformPolicy(cfg);

    auto handleSelection =
        [&](const perastage::command::text::SelectionCommand &command) {
          const bool fixtures =
              command.target ==
              perastage::command::text::SelectionTarget::Fixtures;
          auto &scene = cfg.GetScene();
          std::vector<std::string> current =
              fixtures ? cfg.GetSelectedFixtures() : std::vector<std::string>();
          auto addId = [&](int id) {
            std::string uid;
            if (fixtures) {
              for (const auto &[u, f] : scene.fixtures)
                if (f.fixtureId == id) {
                  uid = u;
                  break;
                }
            } else {
              for (const auto &[u, t] : scene.trusses)
                if (t.unitNumber == id) {
                  uid = u;
                  break;
                }
            }
            if (!uid.empty() &&
                std::find(current.begin(), current.end(), uid) == current.end())
              current.push_back(uid);
          };
          auto removeId = [&](int id) {
            auto it = current.begin();
            while (it != current.end()) {
              int fid = -1;
              if (fixtures) {
                auto fit = scene.fixtures.find(*it);
                if (fit != scene.fixtures.end())
                  fid = fit->second.fixtureId;
              } else {
                auto fit = scene.trusses.find(*it);
                if (fit != scene.trusses.end())
                  fid = fit->second.unitNumber;
              }
              if (fid == id)
                it = current.erase(it);
              else
                ++it;
            }
          };
          for (const auto &operation : command.operations) {
            for (int id = operation.firstId; id <= operation.lastId; ++id) {
              if (operation.kind ==
                  perastage::command::text::SelectionOperationKind::Add)
                addId(id);
              else
                removeId(id);
            }
          }
          if (fixtures) {
            cfg.SetSelectedFixtures(current);
            if (FixtureTablePanel::Instance())
              FixtureTablePanel::Instance()->SelectByUuid(current);
          } else {
            cfg.SetSelectedTrusses(current);
            if (TrussTablePanel::Instance())
              TrussTablePanel::Instance()->SelectByUuid(current);
          }
          if (Viewer2DPanel::Instance())
            Viewer2DPanel::Instance()->SetSelectedUuids(current);
          if (Viewer3DPanel::Instance()) {
            Viewer3DPanel::Instance()->SetSelectedFixtures(current);
            Viewer3DPanel::Instance()->Refresh();
          }
        };

    auto refreshSelectionAfterTransform = [&]() {
      const auto selFixtures = cfg.GetSelectedFixtures();
      const auto selTrusses = cfg.GetSelectedTrusses();
      const auto selSupports = cfg.GetSelectedSupports();
      const auto selSceneObjects = cfg.GetSelectedSceneObjects();

      if (!selFixtures.empty() && FixtureTablePanel::Instance()) {
        FixtureTablePanel::Instance()->ReloadData();
        FixtureTablePanel::Instance()->SelectByUuid(selFixtures, false);
      }
      if (!selTrusses.empty() && TrussTablePanel::Instance()) {
        TrussTablePanel::Instance()->ReloadData();
        TrussTablePanel::Instance()->SelectByUuid(selTrusses, false);
      }
      if (!selSupports.empty() && HoistTablePanel::Instance()) {
        HoistTablePanel::Instance()->ReloadData();
        HoistTablePanel::Instance()->SelectByUuid(selSupports, false);
      }
      if (!selSceneObjects.empty() && SceneObjectTablePanel::Instance()) {
        SceneObjectTablePanel::Instance()->ReloadData();
        SceneObjectTablePanel::Instance()->SelectByUuid(selSceneObjects, false);
      }

      std::vector<std::string> mergedSelection;
      const auto appendSelection = [&](const std::vector<std::string> &source) {
        mergedSelection.insert(mergedSelection.end(), source.begin(),
                               source.end());
      };
      appendSelection(selFixtures);
      appendSelection(selTrusses);
      appendSelection(selSupports);
      appendSelection(selSceneObjects);

      if (Viewer3DPanel::Instance()) {
        Viewer3DPanel::Instance()->SetSelectedFixtures(mergedSelection);
        Viewer3DPanel::Instance()->UpdateScene();
        Viewer3DPanel::Instance()->Refresh();
      }
      if (Viewer2DPanel::Instance())
        Viewer2DPanel::Instance()->SetSelectedUuids(mergedSelection);
    };

    const auto parsed = perastage::command::text::ParseCommandLine(cmd);
    for (const auto &parsedCommand : parsed.commands) {
      if (std::holds_alternative<perastage::command::text::ClearCommand>(
              parsedCommand)) {
        cfg.PushUndoState("cli clear");
        cfg.SetSelectedFixtures({});
        cfg.SetSelectedTrusses({});
        cfg.SetSelectedSceneObjects({});
        if (FixtureTablePanel::Instance())
          FixtureTablePanel::Instance()->SelectByUuid({});
        if (TrussTablePanel::Instance())
          TrussTablePanel::Instance()->SelectByUuid({});
        if (SceneObjectTablePanel::Instance())
          SceneObjectTablePanel::Instance()->SelectByUuid({});
        if (Viewer3DPanel::Instance()) {
          Viewer3DPanel::Instance()->SetSelectedFixtures({});
          Viewer3DPanel::Instance()->Refresh();
        }
        if (Viewer2DPanel::Instance())
          Viewer2DPanel::Instance()->SetSelectedUuids({});
        continue;
      }

      if (const auto *selection =
              std::get_if<perastage::command::text::SelectionCommand>(
                  &parsedCommand)) {
        handleSelection(*selection);
        continue;
      }

      const auto &transform =
          std::get<perastage::command::text::TransformCommand>(parsedCommand);
      scene_grouping::ObjectSelection commandSelection{
          .fixtures = cfg.GetSelectedFixtures(),
          .trusses = cfg.GetSelectedTrusses(),
          .supports = cfg.GetSelectedSupports(),
          .sceneObjects = cfg.GetSelectedSceneObjects()};
      ConsoleProjectMutationHost mutationHost(cfg);
      perastage::command::ExecutionContext context{
          cfg.GetScene(), commandSelection, mutationHost};
      const auto result = perastage::command::transform::Execute(
          perastage::command::text::AdaptTransform(transform), context,
          interactiveTransformPolicy);
      for (const auto &diagnostic : result.diagnostics)
        AppendMessage(FormatCommandDiagnostic(diagnostic));
      if (!result.Success())
        return;
      if (result.mutation.sceneChanged)
        refreshSelectionAfterTransform();
    }

    if (!parsed.Success()) {
      for (const auto &diagnostic : parsed.diagnostics)
        AppendMessage(FormatParseDiagnostic(diagnostic));
      return;
    }

    AppendMessage("[INFO] OK");
  } catch (const std::exception &e) {
    AppendMessage("[ERROR] " + wxString::FromUTF8(e.what()));
  }
}
