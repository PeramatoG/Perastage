#include "fixture_label_preview.h"
#include "symbols/svg_symbol_presentation.h"

void FixtureLabelPreview::RebuildComposition() {
  if (!fonts_) { composition_ = {}; return; }
  if (symbol_) {
    const auto anchor = symbols::SvgPresentationAnchor(*symbol_);
    symbolAnchor_ = {anchor[0],anchor[1]};
  }
  auto previewStyles = styles_;
  displayedStyles_ = viewer2d::ResolveEditableFieldStyles(cfg_, styles_, fixture_.uuid, view_, *fonts_);
  for (int field = 0; field < 3; ++field)
    previewStyles[fixture_.uuid].fieldStyles[view_][field] = displayedStyles_[field];
  composition_ = viewer2d::ComposeFixtureLabels(cfg_, previewStyles, fixture_, view_, bounds_, *fonts_);
}

viewer2d::LabelFieldStyle FixtureLabelPreview::ResolveEditingStyle(
    const viewer2d::FixtureLabelOverrideMap &styles, const std::string &uuid,
    int view, int field) const {
  if (!fonts_) return viewer2d::ResolveFieldStyle(cfg_, styles, uuid, view, field);
  return viewer2d::ResolveEditableFieldStyles(cfg_, styles, uuid, view, *fonts_)[field];
}
