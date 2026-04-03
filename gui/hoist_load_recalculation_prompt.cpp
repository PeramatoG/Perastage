#include "hoist_load_recalculation_prompt.h"

#include "configmanager.h"
#include "hoist_weight_distribution.h"
#include "hoisttablepanel.h"
#include "rigging_extra_weight_settings.h"

#include <algorithm>
#include <vector>
#include <wx/msgdlg.h>

namespace {

constexpr const char *kUnassignedPosition = "Unassigned";

std::string NormalizePositionName(const std::string &positionName) {
  return positionName.empty() ? kUnassignedPosition : positionName;
}

} // namespace

namespace HoistLoadRecalculationPrompt {

bool PromptAndApply(ConfigManager &cfg, wxWindow *parent,
                    const std::unordered_set<std::string> &positionNames,
                    const bool reloadHoistTable) {
  if (positionNames.empty())
    return false;

  auto &scene = cfg.GetScene();
  std::unordered_set<std::string> supportsInPositions;
  supportsInPositions.reserve(scene.supports.size());
  for (const auto &[supportUuid, support] : scene.supports) {
    const std::string supportPosition = NormalizePositionName(support.positionName);
    if (positionNames.find(supportPosition) != positionNames.end())
      supportsInPositions.insert(supportUuid);
  }

  if (supportsInPositions.empty())
    return false;

  wxString message;
  if (positionNames.size() == 1) {
    message = wxString::Format(
        "Position \"%s\" has hoists.\n\nDo you want to recalculate hoist loads "
        "using the updated rounded rigging total?",
        wxString::FromUTF8(positionNames.begin()->c_str()));
  } else {
    message = wxString::Format(
        "%zu positions with hoists were updated.\n\nDo you want to recalculate "
        "hoist loads using the updated rounded rigging totals?",
        positionNames.size());
  }

  const int answer = wxMessageBox(message, "Recalculate hoist loads",
                                  wxYES_NO | wxICON_QUESTION, parent);
  if (answer != wxYES)
    return false;

  const auto extraWeights = RiggingExtraWeightSettings::ParseEntries(
      cfg.GetValue(RiggingExtraWeightSettings::ConfigKey()));
  const auto roundedTotalsByPosition =
      HoistWeightDistribution::BuildRoundedRiggingTotalByHangPosition(
          scene,
          RiggingExtraWeightSettings::BuildKilogramsByPosition(extraWeights));

  std::vector<std::string> supportUuids(supportsInPositions.begin(),
                                        supportsInPositions.end());
  std::sort(supportUuids.begin(), supportUuids.end());
  HoistWeightDistribution::ApplyForImportedSupports(scene, supportUuids,
                                                     roundedTotalsByPosition);

  if (reloadHoistTable && HoistTablePanel::Instance())
    HoistTablePanel::Instance()->ReloadData();

  return true;
}

} // namespace HoistLoadRecalculationPrompt
