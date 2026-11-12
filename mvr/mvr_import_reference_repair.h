/* This file is part of Perastage. */
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
namespace tinyxml2 {
class XMLElement;
}
class MvrScene;
namespace mvr {
void IsolateRetainedAmbiguousReferences(
    MvrScene &, const std::vector<std::string> &rawOccurrenceIds);
// Uses the reader's exact occurrence decisions before raw identities disappear.
void IsolateAmbiguousImportedReferences(
    tinyxml2::XMLElement &, MvrScene &,
    const std::unordered_map<tinyxml2::XMLElement *, std::string> &);
} // namespace mvr
