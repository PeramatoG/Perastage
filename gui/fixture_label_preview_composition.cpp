#include "fixture_label_preview.h"
#include "symbols/svg_symbol_presentation.h"

void FixtureLabelPreview::RebuildComposition() {
  if (!fonts_) { composition_ = {}; return; }
  if (symbol_) {
    const auto anchor = symbols::SvgPresentationAnchor(*symbol_);
    symbolAnchor_ = {anchor[0],anchor[1]};
  }
  const auto current = typography::GetFontCatalog(fonts_->resourceRoot);
  if (current != fonts_->catalog) {
    const auto root = fonts_->resourceRoot;
    fonts_.emplace(root);
  }
  displayedStyles_ = viewer2d::ResolveEditableFieldStyles(cfg_, styles_, fixture_.uuid, view_, *fonts_, &fixture_);
  composition_ = viewer2d::ComposeFixtureLabels(cfg_, styles_, fixture_, view_, bounds_, *fonts_);
}

viewer2d::LabelFieldStyle FixtureLabelPreview::ResolveEditingStyle(
    const viewer2d::FixtureLabelOverrideMap &styles, const std::string &uuid,
    int view, int field) const {
  if (!fonts_) return viewer2d::ResolveFieldStyle(cfg_, styles, uuid, view, field);
  return viewer2d::ResolveEditableFieldStyles(cfg_, styles, uuid, view, *fonts_)[field];
}
