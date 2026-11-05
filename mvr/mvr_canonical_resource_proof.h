/* This file is part of Perastage. */
#pragma once
#include <filesystem>
#include <string>
namespace mvr {
// Proof is issued only after validating a private canonical resource
// derivative. Both byte digests are checked again at the final binding
// validation boundary.
struct CanonicalGdtfProof {
  std::filesystem::path source, output;
  std::string sourceSha256, outputSha256;
};
} // namespace mvr
