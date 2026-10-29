/* This file is part of Perastage. */
#pragma once
#include "mvrscene.h"
#include <filesystem>
#include <functional>
struct FixtureNetworkTestServices {
  std::function<std::string(const std::string &)> source;
  std::function<void(const std::filesystem::path &, const std::string &)> archive;
  std::function<MvrScene(const std::filesystem::path &)> import;
  std::function<void(const MvrScene &, const std::filesystem::path &)> exportScene;
};
void RunFixtureNetworkCases(const std::filesystem::path &dir, const MvrScene &scene,
                           const FixtureNetworkTestServices &services);
