#include "fixture_label_preview.h"
#include "symbols/svg_symbol_presentation.h"

void FixtureLabelPreview::RebuildComposition() {
  if (!fonts_) { composition_ = {}; return; }
  if (symbol_) {
    const auto anchor = symbols::SvgPresentationAnchor(*symbol_);
    symbolAnchor_ = {anchor[0],anchor[1]};
  }
  const auto &bounds = bounds_;
  // Estimate legacy group stacking without migrating its saved settings.
  auto previewStyles = styles_;
  auto legacyStyles = styles_;
  for (auto &[uuid,entry] : legacyStyles) entry.fieldStyles = {};
  std::array<viewer2d::LabelFieldStyle,3> legacy;
  double groupHeight = 0;
  for (int f = 0; f < 3; ++f) {
    legacy[f] = viewer2d::ResolveFieldStyle(cfg_, legacyStyles, fixture_.uuid, view_, f);
    if (legacy[f].visible) {
      const auto &font = legacy[f].bold ? fonts_->bold : fonts_->regular;
      groupHeight += (font.ascent + std::abs(font.descent)) * legacy[f].size / font.unitsPerEm;
    }
  }
  double top = groupHeight / 2;
  for (int f = 0; f < 3; ++f) {
    auto &s = legacy[f];
    const auto &font = s.bold ? fonts_->bold : fonts_->regular;
    const double ascent = font.ascent * s.size / font.unitsPerEm;
    const double height = (font.ascent + std::abs(font.descent)) * s.size / font.unitsPerEm;
    s.y += f * s.size + top - ascent;
    if (s.visible) top -= height;
    if (!viewer2d::HasFieldPresentation(styles_, fixture_.uuid, view_, f))
      previewStyles[fixture_.uuid].fieldStyles[view_][f] = s;
  }
  for (int field=0;field<3;++field)
    displayedStyles_[field] = viewer2d::ResolveFieldStyle(cfg_, previewStyles, fixture_.uuid, view_, field);
  composition_ = viewer2d::ComposeFixtureLabels(cfg_, previewStyles, fixture_, view_, bounds, *fonts_);
}
