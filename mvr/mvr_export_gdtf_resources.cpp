/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "mvr_export_resource_collection.h"

#include "gdtf_canonicalizer.h"
#include "gdtf_mutation_audit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>

#include <tinyxml2.h>

namespace fs = std::filesystem;

namespace {

constexpr const char *kPhysicalPropertiesRevisionText =
    "Updated physical properties for Perastage MVR export";

// Converts an sRGB hex value into the GDTF CIE color representation.
std::string HexToCie(const std::string &hex) {
  if (hex.size() != 7 || hex[0] != '#')
    return {};
  unsigned int rgb = 0;
  std::istringstream input(hex.substr(1));
  input >> std::hex >> rgb;
  const unsigned int red = (rgb >> 16) & 0xFF;
  const unsigned int green = (rgb >> 8) & 0xFF;
  const unsigned int blue = rgb & 0xFF;
  const auto inverseGamma = [](double component) {
    return component <= 0.04045
               ? component / 12.92
               : std::pow((component + 0.055) / 1.055, 2.4);
  };
  const double r = inverseGamma(red / 255.0);
  const double g = inverseGamma(green / 255.0);
  const double b = inverseGamma(blue / 255.0);
  const double xValue = 0.4124 * r + 0.3576 * g + 0.1805 * b;
  const double yValue = 0.2126 * r + 0.7152 * g + 0.0722 * b;
  const double zValue = 0.0193 * r + 0.1192 * g + 0.9505 * b;
  const double sum = xValue + yValue + zValue;
  const double x = sum > 0.0 ? xValue / sum : 0.0;
  const double y = sum > 0.0 ? yValue / sum : 0.0;
  std::ostringstream output;
  output << std::fixed << std::setprecision(6) << x << ',' << y << ','
         << yValue;
  return output.str();
}

// Inserts a FixtureType child at its standard GDTF schema position.
tinyxml2::XMLElement *InsertFixtureTypeChildInOrder(
    tinyxml2::XMLElement *fixtureType, tinyxml2::XMLDocument &document,
    const char *name) {
  tinyxml2::XMLElement *node = document.NewElement(name);
  static constexpr const char *order[] = {
      "AttributeDefinitions", "Wheels", "PhysicalDescriptions", "Models",
      "Geometries", "DMXModes", "Revisions", "FTPresets", "Protocols"};
  int targetIndex = -1;
  for (int index = 0; index < static_cast<int>(std::size(order)); ++index) {
    if (std::string(name) == order[index]) {
      targetIndex = index;
      break;
    }
  }
  tinyxml2::XMLElement *previous = nullptr;
  if (targetIndex >= 0) {
    for (tinyxml2::XMLElement *child = fixtureType->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
      for (int index = targetIndex + 1;
           index < static_cast<int>(std::size(order)); ++index) {
        if (std::string(child->Name()) == order[index]) {
          tinyxml2::XMLNode *inserted =
              previous ? fixtureType->InsertAfterChild(previous, node)
                       : fixtureType->InsertFirstChild(node);
          return inserted ? inserted->ToElement() : nullptr;
        }
      }
      previous = child;
    }
  }
  tinyxml2::XMLNode *inserted = fixtureType->InsertEndChild(node);
  return inserted ? inserted->ToElement() : nullptr;
}

// Applies an explicit export rewrite request to a GDTF description document.
bool ApplyRewriteRequest(tinyxml2::XMLDocument &document,
                         const mvr_export_resources::GdtfRewriteRequest &request) {
  tinyxml2::XMLElement *fixtureType = document.FirstChildElement("GDTF");
  if (fixtureType)
    fixtureType = fixtureType->FirstChildElement("FixtureType");
  else
    fixtureType = document.FirstChildElement("FixtureType");
  if (!fixtureType)
    return false;

  bool patched = false;
  if (!request.color.empty()) {
    tinyxml2::XMLElement *models = fixtureType->FirstChildElement("Models");
    if (models) {
      const std::string cie = HexToCie(request.color);
      for (tinyxml2::XMLElement *model = models->FirstChildElement("Model");
           model; model = model->NextSiblingElement("Model")) {
        model->SetAttribute("Color", cie.c_str());
      }
      patched = true;
    }
  }
  const std::optional<float> weight =
      request.hasWeightKg ? std::optional<float>(request.weightKg) : std::nullopt;
  const std::optional<float> power =
      request.hasPowerW ? std::optional<float>(request.powerW) : std::nullopt;
  patched = GdtfMutationAudit::ApplyPhysicalProperties(
                fixtureType, document, weight, power) ||
            patched;
  if (!request.manufacturer.empty()) {
    fixtureType->SetAttribute("Manufacturer", request.manufacturer.c_str());
    patched = true;
  }
  if (!request.model.empty()) {
    fixtureType->SetAttribute("Name", request.model.c_str());
    patched = true;
  }
  if (request.hasLengthMm || request.hasWidthMm || request.hasHeightMm) {
    tinyxml2::XMLElement *models = fixtureType->FirstChildElement("Models");
    tinyxml2::XMLElement *model =
        models ? models->FirstChildElement("Model") : nullptr;
    if (!models)
      models = InsertFixtureTypeChildInOrder(fixtureType, document, "Models");
    if (!model)
      model = models->InsertNewChildElement("Model");
    if (request.hasLengthMm)
      model->SetAttribute("Length", request.lengthMm / 1000.0f);
    if (request.hasWidthMm)
      model->SetAttribute("Width", request.widthMm / 1000.0f);
    if (request.hasHeightMm)
      model->SetAttribute("Height", request.heightMm / 1000.0f);
    patched = true;
  }
  if (patched) {
    GdtfMutationAudit::AppendRevision(
        fixtureType, document,
        (request.hasWeightKg || request.hasPowerW)
            ? kPhysicalPropertiesRevisionText
            : "Patched fixture metadata for MVR export",
        GdtfMutationAudit::BuildPerastageModifiedBy());
  }
  return true;
}

} // namespace

namespace mvr_export_resources {

// Patches and canonicalizes referenced GDTFs into package-ready plan entries.
GdtfPreparationResult ResourceCollection::PrepareGdtfResources(
    const std::unordered_map<std::string, GdtfRewriteRequest> &rewriteRequests) {
  GdtfPreparationResult result;
  for (ResourceEntry &entry : m_plan.entries) {
    if (!fs::exists(entry.sourcePath))
      continue;
    const auto rewrite = rewriteRequests.find(entry.archivePath);
    if (rewrite != rewriteRequests.end()) {
      runtime_storage::TemporaryWorkspace patchWorkspace(
          "mvr-export-gdtf-patch");
      const fs::path patchedPath =
          patchWorkspace.IsValid()
              ? patchWorkspace.Path().parent_path() /
                    (patchWorkspace.Path().filename().string() + ".gdtf")
              : fs::path{};
      const GdtfCanonicalizer::Result patchResult =
          GdtfCanonicalizer::RewriteArchiveDescription(
              entry.sourcePath, patchedPath,
              [&](tinyxml2::XMLDocument &document) {
                return ApplyRewriteRequest(document, rewrite->second);
              });
      if (!patchResult.success) {
        if (m_diagnosticSink) {
          const std::string resourceName =
              SanitizeArchiveFileName(entry.archivePath, "fixture.gdtf");
          m_diagnosticSink(
              {MvrExportDiagnosticCode::GdtfPatchFailed,
               MvrExportDiagnosticSeverity::Error,
               MvrExportDiagnosticImpact::ExportFailed, true, {}, {}, {},
               resourceName,
               "MVR export could not create the patched GDTF '" +
                   resourceName + "'."});
        }
        result.failureOperation = "PatchGdtf";
        result.failureArchivePath = entry.archivePath;
        result.failureSourcePath = entry.sourcePath.string();
        result.failureReason = "patching did not produce an archive";
        return result;
      }
      entry.sourcePath = patchedPath;
      if (entry.provenance != ResourceProvenance::CompatibilityFallback)
        entry.provenance = ResourceProvenance::StandardGenerated;
      AdoptGeneratedResource(patchedPath);
    }

    if (entry.kind != ResourceKind::Gdtf &&
        entry.sourcePath.extension() != ".gdtf") {
      continue;
    }
    runtime_storage::TemporaryWorkspace canonicalWorkspace(
        "mvr-export-canonical");
    const fs::path canonicalPath =
        canonicalWorkspace.IsValid()
            ? canonicalWorkspace.Path() /
                  SanitizeArchiveFileName(entry.archivePath, "fixture.gdtf")
            : fs::path{};
    GdtfCanonicalizer::Options options;
    options.allowFixtureTypeIdRepair = true;
    options.stableIdSeed =
        entry.archivePath + "|" + entry.sourcePath.string();
    options.sourceLabel =
        entry.archivePath + " from " + entry.sourcePath.string();
    const GdtfCanonicalizer::Result canonicalResult =
        GdtfCanonicalizer::CanonicalizeArchive(entry.sourcePath, canonicalPath,
                                                options);
    if (!canonicalResult.success) {
      if (m_diagnosticSink) {
        for (const std::string &error : canonicalResult.errors) {
          m_diagnosticSink(
              {MvrExportDiagnosticCode::CanonicalizationFailed,
               MvrExportDiagnosticSeverity::Error,
               MvrExportDiagnosticImpact::ExportFailed, true, {}, {}, {},
               fs::path(entry.archivePath).filename().generic_string(),
               "GDTF canonicalization failed: " + error});
        }
      }
      result.failureOperation = "CanonicalizeGdtf";
      result.failureArchivePath = entry.archivePath;
      result.failureSourcePath = entry.sourcePath.string();
      result.failureReason = "canonicalizer reported errors";
      return result;
    }
    entry.sourcePath = canonicalPath;
    if (entry.provenance != ResourceProvenance::CompatibilityFallback)
      entry.provenance = ResourceProvenance::StandardGenerated;
    AdoptGeneratedResource(canonicalPath);
    AdoptWorkspace(std::move(canonicalWorkspace));
  }
  result.success = true;
  result.plan = m_plan;
  return result;
}

} // namespace mvr_export_resources
