#pragma once

#include <functional>
#include <utility>

namespace tools {

// Prepares an installed scene for replacement when a render scope exits.
class ScopedPrepareSceneReplacement {
public:
  using Action = std::function<void()>;

  // Retains the preparation action until the installed scene scope exits.
  explicit ScopedPrepareSceneReplacement(Action prepare)
      : prepare_(std::move(prepare)) {}

  ScopedPrepareSceneReplacement(const ScopedPrepareSceneReplacement &) = delete;
  ScopedPrepareSceneReplacement &
  operator=(const ScopedPrepareSceneReplacement &) = delete;

  // Prepares replacement before the installed scene data source is released.
  ~ScopedPrepareSceneReplacement() { prepare_(); }

private:
  Action prepare_;
};

// Pairs scene-replacement transitions and restores the active scene on scope
// exit.
class ScopedSceneReplacementLifecycle {
public:
  using Action = std::function<void()>;

  // Starts a replacement interval and retains the actions needed for
  // restoration.
  ScopedSceneReplacementLifecycle(Action prepare, Action complete,
                                  Action restoreActiveScene)
      : prepare_(std::move(prepare)), complete_(std::move(complete)),
        restoreActiveScene_(std::move(restoreActiveScene)) {
    PrepareForReplacement();
  }

  ScopedSceneReplacementLifecycle(const ScopedSceneReplacementLifecycle &) =
      delete;
  ScopedSceneReplacementLifecycle &
  operator=(const ScopedSceneReplacementLifecycle &) = delete;

  // Completes any outstanding interval and restores the active scene.
  ~ScopedSceneReplacementLifecycle() {
    CompleteReplacement();
    restoreActiveScene_();
  }

  // Completes the current replacement interval before rendering installed data.
  void CompleteReplacement() {
    if (!replacementPending_)
      return;
    complete_();
    replacementPending_ = false;
  }

  // Starts another replacement interval before the installed data source
  // changes.
  void PrepareForReplacement() {
    if (replacementPending_)
      return;
    prepare_();
    replacementPending_ = true;
  }

  // Creates an exit guard that prepares replacement before snapshot teardown.
  ScopedPrepareSceneReplacement PrepareOnScopeExit() {
    return ScopedPrepareSceneReplacement([this]() { PrepareForReplacement(); });
  }

private:
  Action prepare_;
  Action complete_;
  Action restoreActiveScene_;
  bool replacementPending_ = false;
};

} // namespace tools
