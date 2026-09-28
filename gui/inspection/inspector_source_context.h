#pragma once

#include "inspection/gdtf_inspection.h"
#include "inspection/mvr_inspection.h"
#include "inspection/resource_inspection.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gui::inspection {

using ImmutablePackageBytes =
    std::shared_ptr<const std::vector<std::uint8_t>>;

// Owns every package fact that must change atomically with the displayed source.
struct DisplayedPackageContext final {
  perastage::inspection::PackageKind packageKind =
      perastage::inspection::PackageKind::Mvr;
  std::filesystem::path filesystemPath;
  ImmutablePackageBytes packageBytes;
  std::string fingerprint;
  std::shared_ptr<const perastage::inspection::MvrInspectionResult> mvr;
  std::shared_ptr<const perastage::inspection::GdtfInspectionResult> gdtf;
  std::shared_ptr<const std::vector<perastage::inspection::ResourceDescriptor>>
      resources;
};

// Identifies a preview request without advancing the active source generation.
struct InspectorPreviewTicket final {
  std::uint64_t sourceGeneration = 0;
  std::uint64_t previewGeneration = 0;
  std::shared_ptr<const DisplayedPackageContext> context;
};

// Coordinates source authority independently from replaceable preview work.
class InspectorRequestCoordinator final {
public:
  std::uint64_t BeginSourceRequest();
  std::uint64_t BeginRetainedSourceRequest();
  bool PublishSource(
      std::uint64_t generation,
      std::shared_ptr<const DisplayedPackageContext> context);
  std::optional<InspectorPreviewTicket> BeginPreview(
      const std::shared_ptr<const DisplayedPackageContext> &context);
  void InvalidatePreview();
  bool AcceptPreview(const InspectorPreviewTicket &ticket) const;
  std::shared_ptr<const DisplayedPackageContext> DisplayedContext() const;
  std::uint64_t SourceGeneration() const;

private:
  std::uint64_t sourceGeneration_ = 0;
  std::uint64_t previewGeneration_ = 0;
  std::shared_ptr<const DisplayedPackageContext> displayedContext_;
};

} // namespace gui::inspection
