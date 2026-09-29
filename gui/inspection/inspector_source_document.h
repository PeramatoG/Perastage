#pragma once

#include <optional>
#include <string>

namespace gui::inspection {

enum class InspectorSourceSyntax { Xml, PlainText };

// Describes one exact source document independently from native controls.
struct InspectorSourceDocument final {
  std::string entryPath;
  std::string exactText;
  InspectorSourceSyntax syntax = InspectorSourceSyntax::Xml;
  bool primary = false;
};

// Retains primary source authority separately from the displayed selection.
class InspectorSourceDocumentState final {
public:
  void SetPrimary(InspectorSourceDocument document);
  void ShowSelected(InspectorSourceDocument document);
  void RestorePrimary();
  void Clear();
  const std::optional<InspectorSourceDocument> &Primary() const;
  const std::optional<InspectorSourceDocument> &Displayed() const;

private:
  std::optional<InspectorSourceDocument> primary_;
  std::optional<InspectorSourceDocument> displayed_;
};

} // namespace gui::inspection
