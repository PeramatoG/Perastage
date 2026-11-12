#pragma once
#include "mvrscene.h"
namespace tinyxml2 {
class XMLDocument;
class XMLElement;
} // namespace tinyxml2
namespace mvr {
// Optional Perastage planning properties use the existing permitted Data scope.
// They do not replace standard identities, bindings, geometry or scene
// ownership.
void WriteFixturePlanningMetadata(tinyxml2::XMLDocument &,
                                  tinyxml2::XMLElement &, const MvrScene &);
void ReadFixturePlanningMetadata(const tinyxml2::XMLElement &, MvrScene &);
} // namespace mvr
