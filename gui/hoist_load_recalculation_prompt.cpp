#include "hoist_load_recalculation_prompt.h"

#include "configmanager.h"
#include "hoist_load_limit_utils.h"
#include "hoist_weight_distribution.h"
#include "hoisttablepanel.h"
#include "rigging_extra_weight_settings.h"
#include "support.h"

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

  const int answer = wxMessageBox(message, _("Recalculate hoist loads"),
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

  size_t nearCapacityCount = 0;
  size_t overloadedCount = 0;
  for (const auto &supportUuid : supportUuids) {
    auto supportIt = scene.supports.find(supportUuid);
    if (supportIt == scene.supports.end())
      continue;
    const auto effective = ResolveEffectiveSupportData(supportIt->second);
    const auto state =
        HoistLoadLimitUtils::Evaluate(supportIt->second.loadKg, effective.capacityKg);
    if (state == HoistLoadLimitUtils::LoadLimitState::NearCapacity)
      ++nearCapacityCount;
    else if (state == HoistLoadLimitUtils::LoadLimitState::AtOrAboveCapacity)
      ++overloadedCount;
  }

  if (nearCapacityCount > 0 || overloadedCount > 0) {
    wxString warningMessage;
    if (overloadedCount > 0 && nearCapacityCount > 0) {
      warningMessage = wxString::Format(
          "%zu hoist(s) reached or exceeded capacity and %zu hoist(s) are within "
          "75 kg of capacity after recalculation.",
          overloadedCount, nearCapacityCount);
    } else if (overloadedCount > 0) {
      warningMessage = wxString::Format(
          "%zu hoist(s) reached or exceeded capacity after recalculation.",
          overloadedCount);
    } else {
      warningMessage = wxString::Format(
          "%zu hoist(s) are within 75 kg of capacity after recalculation.",
          nearCapacityCount);
    }
    wxMessageBox(warningMessage, _("Hoist load warning"), wxOK | wxICON_WARNING,
                 parent);
  }

  if (reloadHoistTable && HoistTablePanel::Instance())
    HoistTablePanel::Instance()->ReloadData();

  return true;
}

} // namespace HoistLoadRecalculationPrompt
