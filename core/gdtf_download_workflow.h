#pragma once

#include "gdtfnet.h"

#include <functional>
#include <string>

namespace gdtf_download_workflow {

using Reauthenticate = std::function<bool()>;

GdtfShareResult DownloadWithExpiredSessionRetry(
    GdtfShareClient &client, const std::string &revisionId,
    const std::string &destination, const Reauthenticate &reauthenticate,
    std::function<void(const GdtfDownloadProgress &)> progressCallback = {},
    std::function<bool()> shouldCancelCallback = {});

} // namespace gdtf_download_workflow
