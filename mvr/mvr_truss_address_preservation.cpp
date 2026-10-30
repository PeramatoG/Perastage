/* This file is part of Perastage. */
#include "mvr_truss_address_preservation.h"
#include "fixture_patch_service.h"
#include "fixture_network_reference.h"
#include "filesystem_path_utils.h"
#include <tinyxml2.h>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <string_view>
namespace mvr {
namespace {
MvrExportDiagnostic Block(const Truss &truss, const std::string &field,
                          const std::string &detail, bool unavailable = false) {
  return {unavailable ? MvrExportDiagnosticCode::ResourceMissing : MvrExportDiagnosticCode::StandardDataPreservationBlocked,
      MvrExportDiagnosticSeverity::Error, MvrExportDiagnosticImpact::ExportFailed,
      true, "Truss", truss.name, truss.uuid, field, detail};
}
std::filesystem::path Source(const MvrScene &scene, const Truss &truss) {
  auto path = PathUtils::PathFromUtf8(truss.gdtfSpec);
  return path.is_absolute() ? path : PathUtils::PathFromUtf8(scene.basePath) / path;
}
bool EqualBytes(const std::filesystem::path &source, const std::filesystem::path &output) {
  std::ifstream left(source, std::ios::binary), right(output, std::ios::binary);
  return left && right && std::equal(std::istreambuf_iterator<char>(left), {},
                                    std::istreambuf_iterator<char>(right), {});
}
}
std::vector<MvrExportDiagnostic> CheckTrussAddressBindings(const MvrScene &scene) {
  std::vector<MvrExportDiagnostic> result;
  for (const auto &[uuid, truss] : scene.trusses) {
    if (!PreserveTrussAddressBinding(truss)) continue;
    if (truss.boundGdtfDimensions && *truss.boundGdtfDimensions !=
        std::array<float, 3>{truss.lengthMm, truss.widthMm, truss.heightMm})
      result.push_back(Block(truss, "GDTFSpec/Dimensions", "Edited GDTF-authoritative dimensions require a verified lossless derivative; the authored definition cannot be replaced by generic generation."));
    if (!truss.perastageAuxGdtfArchivePath.empty()) {
      auto auxiliary = truss; auxiliary.gdtfSpec = truss.perastageAuxGdtfArchivePath;
      std::error_code error;
      if (!std::filesystem::is_regular_file(Source(scene, auxiliary), error))
        result.push_back(Block(truss, "AuxGdtf", "The explicitly owned auxiliary resource is unavailable.", true));
    }
    if (const auto addresses = perastage::patch::InspectFixtureAddresses(truss); !addresses)
      result.push_back(Block(truss, "Addresses", addresses.detail));
    if (truss.gdtfSpec.empty()) {
      if (!truss.gdtfMode.empty() || !truss.dmxAddresses->networks.empty())
        result.push_back(Block(truss, "GDTFSpec", "Standard modes and Network assignments require a standard GDTF binding; auxiliary definitions are not substitutes."));
      continue;
    }
    if (truss.gdtfMode.empty())
      result.push_back(Block(truss, "GDTFMode", "MVR 1.6 Table 28 requires an explicit mode with GDTFSpec; no mode is guessed."));
    std::error_code error;
    const auto path = Source(scene, truss);
    if (!std::filesystem::is_regular_file(path, error)) {
      result.push_back(Block(truss, "GDTFSpec", "The authored Truss GDTF is unavailable; its assignments are retained and publication is blocked.", true));
      continue;
    }
    const auto reference = fixture_network::InspectTrussReferences(truss, path);
    if (!reference)
      result.push_back(Block(truss, reference.conflict == fixture_network::Conflict::InvalidBinding ? "GDTFSpec/GDTFMode" : "Addresses/Network", reference.detail,
          reference.conflict == fixture_network::Conflict::UnavailableGdtf));
  }
  return result;
}
std::vector<MvrExportDiagnostic> CheckSerializedTrussAddressBindings(
    const tinyxml2::XMLElement &root, const MvrScene &scene,
    const std::function<std::filesystem::path(const std::string &)> &resource) {
  std::vector<MvrExportDiagnostic> result;
  const auto visit = [&](auto &&self, const tinyxml2::XMLElement &node) -> void {
    if (std::string_view(node.Name()) == "UserData") return;
    const auto *uuid = node.Attribute("uuid");
    if (std::string_view(node.Name()) == "Truss" && uuid && scene.trusses.contains(uuid)) {
      const auto &truss = scene.trusses.at(uuid);
      if (PreserveTrussAddressBinding(truss)) {
        const auto *spec = node.FirstChildElement("GDTFSpec");
        const auto *mode = node.FirstChildElement("GDTFMode");
        const std::string outputSpec = spec && spec->GetText() ? spec->GetText() : "";
        const std::string outputMode = mode && mode->GetText() ? mode->GetText() : "";
        if (outputSpec.empty() != truss.gdtfSpec.empty() || outputMode != truss.gdtfMode)
          result.push_back(Block(truss, "GDTFSpec", "Serialized Truss binding differs from its standard source owner."));
        else if (!outputSpec.empty()) {
          const auto path = resource(outputSpec);
          if (!EqualBytes(Source(scene, truss), path))
            result.push_back(Block(truss, "GDTFSpec", "Serialized Truss GDTF was replaced or rewritten without an authorized derivative operation."));
          const auto references = fixture_network::InspectTrussReferences(truss, path);
          if (!references) result.push_back(Block(truss, "Addresses/Network", references.detail));
        }
      }
    }
    for (auto *child = node.FirstChildElement(); child; child = child->NextSiblingElement()) self(self, *child);
  };
  visit(visit, root);
  return result;
}
}
