#pragma once

namespace perastage::command::transform {

inline constexpr char kPositionCommandId[] = "scene.transform.position";
inline constexpr char kRotationCommandId[] = "scene.transform.rotation";

} // namespace perastage::command::transform

namespace perastage::command::selection {

inline constexpr const char *kUpdateCommandId = "scene.selection.update";
inline constexpr const char *kClearCommandId = "scene.selection.clear";

} // namespace perastage::command::selection

namespace perastage::command::scene_tools {

inline constexpr const char *kGroupCreateCommandId = "scene.group.create";
inline constexpr const char *kGroupUngroupCommandId = "scene.group.ungroup";
inline constexpr const char *kFixtureToSupportCommandId =
    "scene.convert.fixture_to_support";
inline constexpr const char *kSceneObjectsToTrussesCommandId =
    "scene.convert.scene_objects_to_trusses";

} // namespace perastage::command::scene_tools
