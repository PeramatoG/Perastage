#include "gdtf_import_matching.h"

#include <cassert>
#include <string>
#include <vector>

namespace matching = mvr::gdtf_import_matching;

struct DownloadRequestProbe {
  std::string requestedFixtureName;
  std::string resolvedFixtureTypeName;
};

// Verifies that original MVR fixture names drive download matching when embedded metadata is generic.
static void VerifyDistinctOriginalNamesBeatSharedPlaceholderMetadata() {
  const std::string placeholderType = "BLED Standard mode 12CH";
  const std::vector<DownloadRequestProbe> requests = {
      {matching::SelectRequestedFixtureName("Aleda K10 B-EYE",
                                            "Fixtures/BLED Standard mode 12CH.gdtf"),
       placeholderType},
      {matching::SelectRequestedFixtureName("Super Storm 1500",
                                            "Fixtures/BLED Standard mode 12CH.gdtf"),
       placeholderType},
  };

  assert(matching::SelectDownloadSearchFixtureName(requests[0].requestedFixtureName,
                                                   requests[0].resolvedFixtureTypeName) ==
         "Aleda K10 B-EYE");
  assert(matching::SelectDownloadSearchFixtureName(requests[1].requestedFixtureName,
                                                   requests[1].resolvedFixtureTypeName) ==
         "Super Storm 1500");
  assert(matching::SelectDownloadSearchFixtureName(requests[0].requestedFixtureName,
                                                   requests[0].resolvedFixtureTypeName) !=
         placeholderType);
  assert(matching::SelectDownloadSearchFixtureName(requests[1].requestedFixtureName,
                                                   requests[1].resolvedFixtureTypeName) !=
         placeholderType);
}

// Verifies that the GDTFSpec basename is used when the fixture node name is unavailable.
static void VerifySpecBasenameFallback() {
  assert(matching::SelectRequestedFixtureName("", "Folder/Super Storm 1500.gdtf") ==
         "Super Storm 1500");
  assert(matching::SelectRequestedFixtureName("\t\n", "Folder\\Aleda K10 B-EYE.gdtf") ==
         "Aleda K10 B-EYE");
}

// Verifies that resolved metadata remains the final fallback when no original MVR identity exists.
static void VerifyResolvedTypeFallback() {
  assert(matching::SelectDownloadSearchFixtureName("", "BLED Standard mode 12CH") ==
         "BLED Standard mode 12CH");
}

// Runs focused coverage for MVR-requested GDTF import matching identity selection.
int main() {
  VerifyDistinctOriginalNamesBeatSharedPlaceholderMetadata();
  VerifySpecBasenameFallback();
  VerifyResolvedTypeFallback();
  return 0;
}
