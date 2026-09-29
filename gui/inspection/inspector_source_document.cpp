#include "inspection/inspector_source_document.h"

#include <utility>

namespace gui::inspection {

// Replaces source authority and immediately displays the new primary document.
void InspectorSourceDocumentState::SetPrimary(InspectorSourceDocument document) {
  document.primary = true;
  primary_ = std::move(document);
  displayed_ = primary_;
}

// Displays an exact selected resource without changing source authority.
void InspectorSourceDocumentState::ShowSelected(InspectorSourceDocument document) {
  document.primary = false;
  displayed_ = std::move(document);
}

// Restores the authoritative package document deterministically.
void InspectorSourceDocumentState::RestorePrimary() { displayed_ = primary_; }

// Removes both source authority and transient presentation.
void InspectorSourceDocumentState::Clear() {
  primary_.reset();
  displayed_.reset();
}

// Returns the authoritative source document when one is committed.
const std::optional<InspectorSourceDocument> &
InspectorSourceDocumentState::Primary() const { return primary_; }

// Returns the document currently presented by the Source pane.
const std::optional<InspectorSourceDocument> &
InspectorSourceDocumentState::Displayed() const { return displayed_; }

} // namespace gui::inspection
