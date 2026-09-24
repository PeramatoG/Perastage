/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "mvr_import_application_read_services.h"

#include "dummyprofilelibrary.h"
#include "geometry_bounds_resolver.h"
#include "layer_service.h"
#include "truss_dimension_resolution.h"

namespace mvr {

// Adapts application resource and model services to the shared parser.
MvrReadEnvironment
MakeApplicationReadEnvironment(MvrImportResourceResolver &resolver) {
  MvrSceneResourceServices resources{
      [&](const std::string &value) {
        return resolver.RemapArchivePath(value);
      },
      [&](const std::string &value) {
        return resolver.NormalizeGdtfSpec(value);
      },
      [&](const std::string &value) {
        return resolver.NormalizeSupportGdtfSpec(value);
      },
      [&](const std::string &value) { return resolver.ResolveGdtfPath(value); },
      [&](const std::string &value) { return resolver.FixtureMetadata(value); },
      [&](const std::string &path, const std::string &mode,
          std::optional<int> count) {
        return resolver.ResolveGdtfMode(path, mode, count);
      },
      [&](const std::string &path, const std::string &mode) {
        return resolver.GdtfModeChannelCount(path, mode);
      },
      [&](const std::string &type) -> std::optional<SceneReadDictionaryEntry> {
        const auto &entry = resolver.DictionaryEntry(type);
        if (!entry)
          return std::nullopt;
        return SceneReadDictionaryEntry{entry->path, entry->mode,
                                        entry->category};
      },
      [&](const std::string &path, Truss &truss) {
        return resolver.LoadTrussDefinition(path, truss);
      },
      [&](const std::string &path) { return resolver.ResolveScenePath(path); },
      [&](const std::string &path) {
        return resolver.NormalizeGeometryFile(path);
      }};
  MvrSceneReadServices::ModelServices model{
      [](const std::filesystem::path &path, std::string *diagnostic) {
        return GeometryBoundsResolver::Resolve(path, diagnostic);
      },
      [](Truss &truss, bool legacyMetadataContext) {
        ResolveTrussDimensionsFromGeometry(truss, legacyMetadataContext);
      },
      [](MvrScene &scene) { (void)layerdomain::ReconcileLegacyLayers(scene); },
      [](const std::string &displayName) -> std::optional<std::string> {
        const auto profile =
            DummyProfileLibrary::FindByDisplayName(displayName);
        return profile ? std::optional<std::string>(profile->id) : std::nullopt;
      }};
  return {std::move(resources), std::move(model)};
}

} // namespace mvr
