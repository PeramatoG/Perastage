#pragma once
#include "mvrscene.h"
#include "symbols/project_fixture_symbols.h"

class ConfigManager {
public:
  MvrScene scene;
  symbols::ProjectFixtureSymbolStore store;
  int dirtyUpdates = 0;
  MvrScene &GetScene() { return scene; }
  symbols::ProjectFixtureSymbolStore &GetProjectFixtureSymbols() { return store; }
  void MarkDirty() { ++dirtyUpdates; }
};
