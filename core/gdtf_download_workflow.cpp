#include "gdtf_download_workflow.h"

namespace gdtf_download_workflow {

// Downloads a revision and retries once after an expired authenticated session.
GdtfShareResult DownloadWithExpiredSessionRetry(
    GdtfShareClient &client, const std::string &revisionId,
    const std::string &destination, const Reauthenticate &reauthenticate,
    std::function<void(const GdtfDownloadProgress &)> progressCallback,
    std::function<bool()> shouldCancelCallback) {
  GdtfShareResult result = client.DownloadRevision(
      revisionId, destination, progressCallback, shouldCancelCallback);
  if (result.category != GdtfShareResultCategory::AuthenticationRejected ||
      !reauthenticate || !reauthenticate()) {
    return result;
  }
  return client.DownloadRevision(revisionId, destination,
                                 std::move(progressCallback),
                                 std::move(shouldCancelCallback));
}

} // namespace gdtf_download_workflow
