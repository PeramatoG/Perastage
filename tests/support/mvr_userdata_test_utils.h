#pragma once
#include <cassert>
#include <string_view>
#include <tinyxml2.h>

namespace mvr_test {
// Provider order is immaterial; duplicate native providers are a contract error.
inline tinyxml2::XMLElement *
FindUniqueProviderData(tinyxml2::XMLElement &userData, std::string_view provider) {
  tinyxml2::XMLElement *found = nullptr;
  for (auto *data = userData.FirstChildElement("Data"); data;
       data = data->NextSiblingElement("Data")) {
    const char *value = data->Attribute("provider");
    if (value && provider == value) {
      assert(!found);
      found = data;
    }
  }
  return found;
}
} // namespace mvr_test
