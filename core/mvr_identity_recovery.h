#pragma once

#include "mvrscene.h"

#include <string>
#include <vector>

namespace mvridentity {

enum class RecoveryReason {
  Canonicalized,
  Missing,
  Malformed,
  Duplicate,
  KeyFieldMismatch,
  InferredLayer
};

struct RecoveryDiagnostic {
  std::string objectKind;
  std::string objectName;
  std::string sourceContext;
  std::string originalIdentity;
  std::string replacementIdentity;
  RecoveryReason reason = RecoveryReason::Missing;
};

struct RecoveryResult {
  std::vector<RecoveryDiagnostic> diagnostics;
};

// Canonicalizes and deterministically repairs all persisted scene identities.
RecoveryResult RecoverSceneIdentities(MvrScene &scene,
                                      const std::string &sourceContext);

// Formats a recovery record as a stable structured diagnostic.
std::string FormatRecoveryDiagnostic(const RecoveryDiagnostic &diagnostic);

} // namespace mvridentity
