#include "preferencesdialog.h"
#include "configmanager.h"
#include "guiconfigservices.h"

PreferencesDialog::PreferencesDialog(wxWindow *parent)
    : PreferencesDialog(
          parent, GetDefaultGuiConfigServices().Preferences(),
          [](const std::function<void()> &apply) {
            auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
            const auto originalDirtyState = cfg.CaptureDirtyState();
            apply();
            const bool saved = cfg.SaveUserConfig();
            cfg.RestoreDirtyState(originalDirtyState);
            return saved;
          }) {}
