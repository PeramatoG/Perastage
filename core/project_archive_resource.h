#pragma once

#include <cstdint>
#include <string>
#include <vector>

// One project-owned payload participating in the transactional PSTG writer.
struct ProjectArchiveResource {
  std::string entryName;
  std::vector<std::uint8_t> bytes;
  bool required = false;
};
