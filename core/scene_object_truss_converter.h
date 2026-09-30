/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

class MvrScene;

struct SceneObjectToTrussConversionResult {
  std::string modelFile;
  std::vector<std::string> convertedUuids;
};

struct SceneObjectToTrussConversionScope {
  std::string modelFile;
  std::vector<std::string> sceneObjectUuids;
};

// Resolves the deterministic same-model conversion scope for one scene object.
SceneObjectToTrussConversionScope ResolveSceneObjectsWithSameModelToTrusses(
    const MvrScene &scene, const std::string &sourceSceneObjectUuid);

// Converts all scene objects in the source object's resolved same-model scope.
SceneObjectToTrussConversionResult ConvertSceneObjectsWithSameModelToTrusses(
    MvrScene &scene, const std::string &sourceSceneObjectUuid);
