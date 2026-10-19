#pragma once
#include "truss.h"

namespace truss_instance {
// Replace type/geometry while preserving the instance's authored identity and state.
inline void ReplaceType(Truss &target, const Truss &replacement) {
  const Truss previous = target;
  target = replacement;
  target.uuid = previous.uuid;
  target.classing = previous.classing;
  target.fixtureIdText = previous.fixtureIdText;
  target.fixtureIdNumeric = previous.fixtureIdNumeric;
  target.identifierSource = previous.identifierSource;
  target.name = previous.name;
  target.layer = previous.layer;
  target.layerUuid = previous.layerUuid;
  target.position = previous.position;
  target.positionName = previous.positionName;
  target.unitNumber = previous.unitNumber;
  target.customId = previous.customId;
  target.customIdType = previous.customIdType;
  target.transform = previous.transform;
  target.localTransform = previous.localTransform;
  target.hasLocalTransform = previous.hasLocalTransform;
  target.parentGroupUuid = previous.parentGroupUuid;
  target.calculatedLoadKg = previous.calculatedLoadKg;
  target.manualLoadKg = previous.manualLoadKg;
  target.hasManualLoadOverride = previous.hasManualLoadOverride;
}

} // namespace truss_instance
