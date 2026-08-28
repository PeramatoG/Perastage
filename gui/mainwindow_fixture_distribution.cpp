#include "mainwindow.h"

#include "consolepanel.h"
#include "fixture_distribution_dialog.h"
#include "fixture_line_distribution.h"
#include "fixturetablepanel.h"
#include "guiconfigservices.h"
#include "truss_attachment_paths.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"

#include <iterator>

namespace {

// Returns a user-facing validation message for a failed line resolution.
const char *ResolveErrorMessage(fixture_line_distribution::ResolveError error) {
  if (error == fixture_line_distribution::ResolveError::TooFewFixtures)
    return wxTRANSLATE("Select at least two fixtures to distribute.");
  if (error == fixture_line_distribution::ResolveError::MissingFixture)
    return wxTRANSLATE("The fixture selection is no longer available.");
  return wxTRANSLATE("The selected fixtures must share one straight line on the same or "
         "connected trusses.");
}

// Converts a world point from millimeters to viewport meters.
std::array<float, 3> ToViewportMeters(const std::array<float, 3> &pointMm) {
  return {pointMm[0] / 1000.0f, pointMm[1] / 1000.0f, pointMm[2] / 1000.0f};
}

// Converts a viewport point from meters to scene millimeters.
std::array<float, 3>
ToSceneMillimeters(const std::array<float, 3> &pointMeters) {
  return {pointMeters[0] * 1000.0f, pointMeters[1] * 1000.0f,
          pointMeters[2] * 1000.0f};
}

// Resolves the same fixture attachment paths used by Magnet guidance.
std::vector<truss_attachment_paths::Path>
ResolveAttachmentPaths(const MvrScene &scene) {
  static truss_attachment_paths::Resolver resolver;
  std::vector<truss_attachment_paths::Path> paths;
  for (const auto &[uuid, truss] : scene.trusses) {
    (void)uuid;
    auto resolution = resolver.Resolve(scene, truss);
    paths.insert(paths.end(), std::make_move_iterator(resolution.paths.begin()),
                 std::make_move_iterator(resolution.paths.end()));
  }
  return paths;
}

// Reports whether a focused child belongs to the requested viewport.
bool ContainsFocusedWindow(const wxWindow *viewport, const wxWindow *focus) {
  for (const wxWindow *current = focus; current;
       current = current->GetParent()) {
    if (current == viewport)
      return true;
  }
  return false;
}

} // namespace

// Opens the unified fixture distribution workflow and routes the selected mode.
void MainWindow::OnDistributeFixtures(wxCommandEvent &event) {
  wxWindow *previousFocus = wxWindow::FindFocus();
  FixtureDistributionDialog dialog(this);
  if (dialog.ShowModal() != wxID_OK)
    return;
  const auto options = dialog.GetOptions();
  dialog.Hide();
  if (previousFocus)
    previousFocus->SetFocus();
  if (options.mode == FixtureDistributionMode::FullTruss) {
    OnDistributeFixturesOnTruss(event);
    return;
  }
  if (options.mode == FixtureDistributionMode::BetweenPoints) {
    OnDistributeFixturesBetweenPoints(event);
    return;
  }

  const wxWindow *focus = wxWindow::FindFocus();
  bool focusIn2D = ContainsFocusedWindow(viewport2DPanel, focus);
  bool focusIn3D = ContainsFocusedWindow(viewportPanel, focus);
  if (!focusIn2D && !focusIn3D) {
    focusIn3D = viewportPanel != nullptr;
    focusIn2D = !focusIn3D && viewport2DPanel != nullptr;
  }
  if (!focusIn2D && !focusIn3D) {
    ReportFixtureDistributionMessage(
        wxTRANSLATE("Open a 2D or 3D scene viewport before distributing fixtures."));
    return;
  }
  IGuiConfigServices &services = GetDefaultGuiConfigServices();
  const auto selection = services.Selection().GetSelectedFixtures();
  const auto resolved = fixture_line_distribution::ResolveSelectedLine(
      services.Project().GetScene(), selection,
      ResolveAttachmentPaths(services.Project().GetScene()));
  if (!resolved.line) {
    ReportFixtureDistributionMessage(ResolveErrorMessage(resolved.error));
    return;
  }
  const auto line = *resolved.line;
  std::vector<float> halfExtents;
  if (options.edgeToEdge && viewportPanel)
    halfExtents =
        viewportPanel->GetFixtureHalfExtentsMm(selection, line.start, line.end);
  if (options.edgeToEdge && halfExtents.size() != selection.size()) {
    ReportFixtureDistributionMessage(
        wxTRANSLATE("Fixture geometry must be loaded before using edge-to-edge spacing."));
    return;
  }
  ReportFixtureDistributionMessage(
      options.fromPoint
          ? wxTRANSLATE("Choose a start point and then a direction on the truss line."
          ): wxTRANSLATE("Choose two limits on the truss line for outside-in distribution."));
  auto complete = [this, selection, line, options, halfExtents](
                      const auto &firstMeters, const auto &secondMeters) {
    if (!firstMeters || !secondMeters) {
      ReportFixtureDistributionMessage(wxTRANSLATE("Fixture distribution cancelled."));
      return;
    }
    auto start = ToSceneMillimeters(*firstMeters);
    auto end = ToSceneMillimeters(*secondMeters);
    if (options.fromPoint) {
      float directionDot = 0.0f;
      for (int axis = 0; axis < 3; ++axis)
        directionDot +=
            (end[axis] - start[axis]) * (line.end[axis] - line.start[axis]);
      end = directionDot >= 0.0f ? line.end : line.start;
    }
    fixture_line_distribution::SpacingOptions spacing;
    spacing.spacingMm = static_cast<float>(options.spacingMeters * 1000.0);
    spacing.reference =
        options.edgeToEdge
            ? fixture_line_distribution::SpacingReference::FixtureEdges
            : fixture_line_distribution::SpacingReference::Centers;
    spacing.origin =
        options.fromPoint
            ? fixture_line_distribution::SpacingOrigin::FromPointInDirection
            : fixture_line_distribution::SpacingOrigin::BetweenPointsOutsideIn;
    spacing.halfExtentsMm = halfExtents;
    IGuiConfigServices &active = GetDefaultGuiConfigServices();
    active.History().PushUndoState("distribute fixtures with exact spacing");
    const auto result = fixture_line_distribution::ApplySpacing(
        active.Project().GetScene(), selection, start, end, spacing);
    if (!result.applied) {
      active.History().Undo();
      if (!result.fits)
        ReportFixtureDistributionMessage(
            wxTRANSLATE("The selected fixtures do not fit on this truss line with the "
                        "requested spacing."));
      else
        ReportFixtureDistributionMessage(
            wxTRANSLATE("Fixture distribution could not be completed."));
      return;
    }
    RefreshAfterToolSceneUpdate();
    RestoreFixtureDistributionSelection(selection);
    ReportFixtureDistributionMessage(
        wxTRANSLATE("Fixtures distributed with the requested spacing."));
  };
  if (focusIn3D)
    viewportPanel->BeginLinePointSelection(
        ToViewportMeters(line.start), ToViewportMeters(line.end), complete);
  else
    viewport2DPanel->BeginLinePointSelection(
        ToViewportMeters(line.start), ToViewportMeters(line.end), complete);
}

// Reports a non-blocking fixture-distribution message in the status and
// console.
void MainWindow::ReportFixtureDistributionMessage(const std::string &message) {
  SetStatusText(wxGetTranslation(wxString::FromUTF8(message)), 0);
  if (consolePanel)
    consolePanel->AppendMessage(wxString::FromUTF8(message));
}

// Restores fixture selection after distribution refreshes all scene views.
void MainWindow::RestoreFixtureDistributionSelection(
    const std::vector<std::string> &selection) {
  CallAfter([this, selection]() {
    GetDefaultGuiConfigServices().Selection().SetSelectedFixtures(selection);
    if (fixturePanel)
      fixturePanel->SelectByUuid(selection, false);
    if (viewportPanel)
      viewportPanel->SetSelectedFixtures(selection);
    if (viewport2DPanel) {
      viewport2DPanel->UpdateScene();
      viewport2DPanel->Refresh();
    }
  });
}

// Distributes selected fixtures across the complete truss with end margins.
void MainWindow::OnDistributeFixturesOnTruss(wxCommandEvent &WXUNUSED(event)) {
  const wxWindow *focus = wxWindow::FindFocus();
  if (!ContainsFocusedWindow(viewport2DPanel, focus) &&
      !ContainsFocusedWindow(viewportPanel, focus)) {
    ReportFixtureDistributionMessage(
        wxTRANSLATE("Activate a 2D or 3D scene viewport before distributing fixtures."));
    return;
  }
  IGuiConfigServices &services = GetDefaultGuiConfigServices();
  const auto selection = services.Selection().GetSelectedFixtures();
  const auto paths = ResolveAttachmentPaths(services.Project().GetScene());
  const auto resolved = fixture_line_distribution::ResolveSelectedLine(
      services.Project().GetScene(), selection, paths);
  if (!resolved.line) {
    ReportFixtureDistributionMessage(ResolveErrorMessage(resolved.error));
    return;
  }
  services.History().PushUndoState("distribute fixtures on truss");
  fixture_line_distribution::Apply(services.Project().GetScene(), selection,
                                   resolved.line->start, resolved.line->end,
                                   true);
  RefreshAfterToolSceneUpdate();
  RestoreFixtureDistributionSelection(selection);
  ReportFixtureDistributionMessage(
      wxTRANSLATE("Fixtures distributed uniformly along the truss line."));
}

// Starts two-point fixture distribution in the focused scene viewport.
void MainWindow::OnDistributeFixturesBetweenPoints(
    wxCommandEvent &WXUNUSED(event)) {
  const wxWindow *focus = wxWindow::FindFocus();
  const bool focusIn2D = ContainsFocusedWindow(viewport2DPanel, focus);
  const bool focusIn3D = ContainsFocusedWindow(viewportPanel, focus);
  if (!focusIn2D && !focusIn3D) {
    ReportFixtureDistributionMessage(
        wxTRANSLATE("Activate a 2D or 3D scene viewport before choosing endpoints."));
    return;
  }
  IGuiConfigServices &services = GetDefaultGuiConfigServices();
  const auto selection = services.Selection().GetSelectedFixtures();
  const auto paths = ResolveAttachmentPaths(services.Project().GetScene());
  const auto resolved = fixture_line_distribution::ResolveSelectedLine(
      services.Project().GetScene(), selection, paths);
  if (!resolved.line) {
    ReportFixtureDistributionMessage(ResolveErrorMessage(resolved.error));
    return;
  }
  const auto line = *resolved.line;
  ReportFixtureDistributionMessage(
      wxTRANSLATE("Choose two points on the truss line, or press Esc to cancel."));
  auto completeSelection = [this, selection](const auto &start,
                                             const auto &end) {
    if (!start || !end) {
      if (consolePanel)
        consolePanel->AppendMessage("Fixture distribution cancelled.");
      SetStatusText(_("Ready"), 0);
      return;
    }
    IGuiConfigServices &active = GetDefaultGuiConfigServices();
    active.History().PushUndoState("distribute fixtures between truss points");
    if (!fixture_line_distribution::Apply(active.Project().GetScene(),
                                          selection, ToSceneMillimeters(*start),
                                          ToSceneMillimeters(*end), false)) {
      active.History().Undo();
      ReportFixtureDistributionMessage(
          wxTRANSLATE("Fixture distribution could not be completed."));
      return;
    }
    RefreshAfterToolSceneUpdate();
    RestoreFixtureDistributionSelection(selection);
    ReportFixtureDistributionMessage(
        wxTRANSLATE("Fixtures distributed between the selected truss points."));
  };

  if (focusIn3D) {
    viewportPanel->BeginLinePointSelection(ToViewportMeters(line.start),
                                           ToViewportMeters(line.end),
                                           completeSelection);
    return;
  }
  viewport2DPanel->BeginLinePointSelection(ToViewportMeters(line.start),
                                           ToViewportMeters(line.end),
                                           completeSelection);
}
