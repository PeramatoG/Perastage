/*
 * This file is part of Perastage.
 */
#include <cassert>
#include "filesystem_path_utils.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <wx/init.h>

#include "dictionary_json_contract.h"
#include "gdtfdictionary.h"
#include "json.hpp"
#include "projectutils.h"

namespace {

std::string ReadFile(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

void WriteFile(const std::filesystem::path &path, const std::string &content) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  assert(out.is_open());
  out << content;
  assert(out.good());
}

} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const std::filesystem::path fixturesDir =
      PathUtils::PathFromUtf8(ProjectUtils::GetDefaultLibraryPath("fixtures"));
  std::filesystem::create_directories(fixturesDir);
  const std::filesystem::path dictPath = fixturesDir / "gdtf_dictionary.json";

  const bool hadOriginal = std::filesystem::exists(dictPath);
  const std::string originalContent = hadOriginal ? ReadFile(dictPath) : std::string{};

  const std::filesystem::path fixtureFile = fixturesDir / "color_fixture.gdtf";
  WriteFile(fixtureFile, "fixture");

  nlohmann::json entries = nlohmann::json::object();
  entries["ColorOnlyType"] = {{"color", "#112233"}};
  entries["FullType"] = {{"file", fixtureFile.string()},
                         {"mode", "ModeA"},
                         {"category", "Wash"},
                         {"color", "#ABCDEF"}};
  WriteFile(dictPath,
            DictionaryJsonContract::MakeRoot("fixtures", std::move(entries)).dump(4));

  auto loadedOpt = GdtfDictionary::Load();
  assert(loadedOpt.has_value());

  const auto colorOnlyIt = loadedOpt->find("ColorOnlyType");
  assert(colorOnlyIt != loadedOpt->end());
  assert(colorOnlyIt->second.color == "#112233");
  assert(colorOnlyIt->second.path.empty());

  const auto fullIt = loadedOpt->find("FullType");
  assert(fullIt != loadedOpt->end());
  assert(fullIt->second.color == "#ABCDEF");

  assert(GdtfDictionary::Save(*loadedOpt));

  nlohmann::json savedRoot;
  {
    std::ifstream in(dictPath);
    assert(in.is_open());
    in >> savedRoot;
  }

  assert(savedRoot.contains("entries"));
  assert(savedRoot["entries"]["ColorOnlyType"]["color"] == "#112233");
  assert(savedRoot["entries"]["FullType"]["color"] == "#ABCDEF");

  std::filesystem::remove(fixtureFile);
  if (hadOriginal)
    WriteFile(dictPath, originalContent);
  else
    std::filesystem::remove(dictPath);

  return 0;
}
