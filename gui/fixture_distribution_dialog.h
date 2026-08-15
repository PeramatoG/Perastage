#pragma once

#include <wx/dialog.h>

class wxChoice;
class wxRadioBox;
class wxSpinCtrlDouble;

enum class FixtureDistributionMode { ExactSpacing, FullTruss, BetweenPoints };

struct FixtureDistributionDialogOptions {
  FixtureDistributionMode mode = FixtureDistributionMode::ExactSpacing;
  double spacingMeters = 0.5;
  bool edgeToEdge = false;
  bool fromPoint = false;
};

class FixtureDistributionDialog final : public wxDialog {
public:
  explicit FixtureDistributionDialog(wxWindow *parent);
  FixtureDistributionDialogOptions GetOptions() const;

private:
  void UpdateOptionAvailability();

  wxChoice *modeChoice_ = nullptr;
  wxSpinCtrlDouble *spacing_ = nullptr;
  wxRadioBox *reference_ = nullptr;
  wxRadioBox *origin_ = nullptr;
};
