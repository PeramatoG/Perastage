#pragma once

// Bounded source provenance carried by the existing scene values and snapshots.
// Selection IDs used by the application remain distinct from standard MVR IDs.
struct MvrIdentifierSource {
  int selectionIdAtImport = 0;
  bool unitNumberPresent = false;
  bool customIdPresent = false;
  bool customIdTypePresent = false;
};
