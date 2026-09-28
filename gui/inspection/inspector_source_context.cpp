#include "inspection/inspector_source_context.h"

#include <utility>

namespace gui::inspection {

// Invalidates displayed interaction state before a new source starts loading.
std::uint64_t InspectorRequestCoordinator::BeginSourceRequest() {
  ++sourceGeneration_;
  ++previewGeneration_;
  displayedContext_.reset();
  return sourceGeneration_;
}

// Publishes a source only when it still owns current source authority.
void InspectorRequestCoordinator::PublishSource(
    std::uint64_t generation,
    std::shared_ptr<const DisplayedPackageContext> context) {
  if (generation == sourceGeneration_)
    displayedContext_ = std::move(context);
}

// Creates preview identity only for the exact currently displayed context.
std::optional<InspectorPreviewTicket>
InspectorRequestCoordinator::BeginPreview(
    const std::shared_ptr<const DisplayedPackageContext> &context) {
  if (!context || context != displayedContext_)
    return std::nullopt;
  return InspectorPreviewTicket{sourceGeneration_, ++previewGeneration_,
                                context};
}

// Accepts a preview only while both its source and sub-generation remain current.
bool InspectorRequestCoordinator::AcceptPreview(
    const InspectorPreviewTicket &ticket) const {
  return ticket.context && ticket.context == displayedContext_ &&
         ticket.sourceGeneration == sourceGeneration_ &&
         ticket.previewGeneration == previewGeneration_;
}

// Returns the immutable package context currently exposed to interactions.
std::shared_ptr<const DisplayedPackageContext>
InspectorRequestCoordinator::DisplayedContext() const {
  return displayedContext_;
}

// Returns current source authority without exposing preview sequencing.
std::uint64_t InspectorRequestCoordinator::SourceGeneration() const {
  return sourceGeneration_;
}

} // namespace gui::inspection
