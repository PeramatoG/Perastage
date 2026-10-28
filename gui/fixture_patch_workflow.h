/* This file is part of Perastage. */
#pragma once
class ConfigManager;
namespace fixture_patch_workflow {
// Publishes the existing auto-patch operation with one successful Undo entry.
bool AutoPatch(ConfigManager &config);
}
