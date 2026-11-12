/* This file is part of Perastage. */
#include "mvr_fixture_planning_metadata.h"
#include "mvr_publication_repair.h"
#include "uuidutils.h"
#include <tinyxml2.h>
#include <map>
#include <cmath>
namespace mvr {
void WriteFixturePlanningMetadata(tinyxml2::XMLDocument &doc,
                                  tinyxml2::XMLElement &provider,
                                  const MvrScene &scene) {
  if (scene.fixtures.empty())
    return;
  auto *map = doc.NewElement("FixturePlanningMetadataMap");
  map->SetAttribute("schemaVersion", 1);
  std::map<std::string, const Fixture *> fixtures;
  for (const auto &[id, fixture] : scene.fixtures)
    fixtures[id] = &fixture;
  for (const auto &[id, fixture] : fixtures) {
    auto *entry = doc.NewElement("FixturePlanning");
    entry->SetAttribute("uuid", id.c_str());
    entry->SetAttribute("powerPresent", fixture->powerConsumptionWPresent);
    const auto set = [&](const char *name, float value) {
      if (std::isfinite(value) && value >= 0)
        entry->SetAttribute(name, publication::PlanningNumber(value).c_str());
    };
    set("weightKg", fixture->weightKg);
    set("powerW", fixture->powerConsumptionW);
    if (fixture->sourcePowerConsumptionW)
      set("sourcePowerW", *fixture->sourcePowerConsumptionW);
    if (fixture->planningPowerOverrideW)
      set("overridePowerW", *fixture->planningPowerOverrideW);
    map->InsertEndChild(entry);
  }
  provider.InsertEndChild(map);
}
void ReadFixturePlanningMetadata(const tinyxml2::XMLElement &root,
                                 MvrScene &scene) {
  const auto *user = root.FirstChildElement("UserData");
  for (auto *provider = user ? user->FirstChildElement("Data") : nullptr;
       provider; provider = provider->NextSiblingElement("Data")) {
    const auto *name = provider->Attribute("provider");
    if (!name || std::string(name) != "Perastage")
      continue;
    const auto *map = provider->FirstChildElement("FixturePlanningMetadataMap");
    if (!map || map->IntAttribute("schemaVersion") != 1)
      continue;
    for (auto *entry = map->FirstChildElement("FixturePlanning"); entry;
         entry = entry->NextSiblingElement("FixturePlanning")) {
      const auto *raw = entry->Attribute("uuid");
      if (!raw)
        continue;
      auto it = scene.fixtures.find(CanonicalizeUuid(raw));
      if (it == scene.fixtures.end())
        continue;
      auto &fixture = it->second;
      const auto read = [&](const char *key) -> std::optional<float> {
        float number = 0;
        return entry->QueryFloatAttribute(key, &number) ==
                           tinyxml2::XML_SUCCESS &&
                       std::isfinite(number) && number >= 0
                   ? std::optional<float>{number}
                   : std::nullopt;
      };
      bool present = false;
      if (entry->QueryBoolAttribute("powerPresent", &present) ==
          tinyxml2::XML_SUCCESS)
        fixture.powerConsumptionWPresent = present;
      if (auto weight = read("weightKg"))
        fixture.weightKg = *weight;
      if (auto source = read("sourcePowerW"))
        fixture.sourcePowerConsumptionW = *source;
      if (auto overrideValue = read("overridePowerW"))
        fixture.SetPlanningPowerOverride(*overrideValue);
      else if (auto power = read("powerW"))
        fixture.powerConsumptionW = *power;
    }
  }
}
} // namespace mvr
