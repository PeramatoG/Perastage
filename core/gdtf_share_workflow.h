#pragma once

#include "credentialstore.h"
#include "gdtfnet.h"

#include <optional>
#include <string>

namespace gdtf_share_workflow {

enum class CatalogSource {
  None,
  Cached,
  Online
};

enum class CredentialAvailability {
  None,
  UsernameHintOnly,
  Complete
};

enum class CredentialPromptReason {
  None,
  MissingCredentials,
  RejectedCredentials,
  ExpiredSession
};

enum class CatalogAccessAction {
  OpenCachedCatalog,
  OpenOnlineCatalog,
  RequestCredentials,
  ReportCatalogFailure,
  Cancel
};

struct WorkflowState {
  CatalogSource catalogSource = CatalogSource::None;
  CredentialAvailability credentialAvailability = CredentialAvailability::None;
  bool sessionAuthenticated = false;
  std::optional<std::string> authenticatedUsername;
  std::optional<GdtfShareResult> lastAuthenticationResult;
  std::optional<GdtfShareResult> lastCatalogResult;
};

// Converts a credential load result to a stable availability state.
inline CredentialAvailability DetermineCredentialAvailability(
    const CredentialStore::LoadResult &loadResult) {
  if (loadResult.credentials && !loadResult.credentials->username.empty() &&
      !loadResult.credentials->password.empty()) {
    return CredentialAvailability::Complete;
  }
  if (loadResult.usernameHint && !loadResult.usernameHint->empty())
    return CredentialAvailability::UsernameHintOnly;
  return CredentialAvailability::None;
}

// Reports whether a result should open a credentials prompt.
inline CredentialPromptReason PromptReasonForAuthenticationResult(
    bool hasCompleteCredentials, const std::optional<GdtfShareResult> &result,
    bool expiredSession = false) {
  if (!hasCompleteCredentials)
    return CredentialPromptReason::MissingCredentials;
  if (expiredSession)
    return CredentialPromptReason::ExpiredSession;
  if (result && result->category == GdtfShareResultCategory::AuthenticationRejected)
    return CredentialPromptReason::RejectedCredentials;
  return CredentialPromptReason::None;
}

// Returns true when the prompt reason requires user credentials.
inline bool ShouldPromptForCredentials(CredentialPromptReason reason) {
  return reason != CredentialPromptReason::None;
}

// Chooses the next user-visible step without coupling catalog state to GUI code.
inline CatalogAccessAction DetermineCatalogAccessAction(
    bool hasUsableCache, CredentialAvailability credentials,
    const std::optional<GdtfShareResult> &authenticationResult,
    bool onlineCatalogAvailable, bool authenticationCancelled = false) {
  if (onlineCatalogAvailable)
    return CatalogAccessAction::OpenOnlineCatalog;
  if (hasUsableCache)
    return CatalogAccessAction::OpenCachedCatalog;
  if (authenticationCancelled)
    return CatalogAccessAction::Cancel;
  if (credentials != CredentialAvailability::Complete ||
      (authenticationResult &&
       authenticationResult->category ==
           GdtfShareResultCategory::AuthenticationRejected)) {
    return CatalogAccessAction::RequestCredentials;
  }
  return CatalogAccessAction::ReportCatalogFailure;
}

// Converts catalog source state to stable diagnostic text.
inline const char *CatalogSourceName(CatalogSource source) {
  switch (source) {
  case CatalogSource::None: return "none";
  case CatalogSource::Cached: return "cache";
  case CatalogSource::Online: return "online";
  }
  return "unknown";
}

// Converts credential availability state to stable diagnostic text.
inline const char *CredentialAvailabilityName(CredentialAvailability availability) {
  switch (availability) {
  case CredentialAvailability::None: return "none";
  case CredentialAvailability::UsernameHintOnly: return "username_hint_only";
  case CredentialAvailability::Complete: return "complete";
  }
  return "unknown";
}

// Converts credential prompt reasons to stable diagnostic text.
inline const char *CredentialPromptReasonName(CredentialPromptReason reason) {
  switch (reason) {
  case CredentialPromptReason::None: return "none";
  case CredentialPromptReason::MissingCredentials: return "missing_credentials";
  case CredentialPromptReason::RejectedCredentials: return "rejected_credentials";
  case CredentialPromptReason::ExpiredSession: return "expired_session";
  }
  return "unknown";
}

} // namespace gdtf_share_workflow
