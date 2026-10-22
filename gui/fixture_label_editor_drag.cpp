#include "fixture_label_editor.h"

void FixtureLabelEditor::OnPreviewDrag(label_preview::DragPhase phase,
                                      const viewer2d::LabelFieldStyle &style) {
  if (phase == label_preview::DragPhase::Begin) {
    dragDraft_ = draft_;
    changedBeforeDrag_ = changed_;
    return;
  }
  if (phase == label_preview::DragPhase::Cancel) {
    if (dragDraft_) {
      draft_ = std::move(*dragDraft_);
      changed_ = changedBeforeDrag_;
    }
    dragDraft_.reset();
    LoadField();
    return;
  }
  if (phase == label_preview::DragPhase::End) {
    dragDraft_.reset();
    return;
  }
  const auto ids = scope_->GetSelection() == 0
      ? std::vector<std::string>{viewer2d::kLabelDefaultsKey} : selection_;
  for (const auto &id : ids)
    draft_[id].fieldStyles[view_->GetSelection()][fields_->GetSelection()] = style;
  changed_ = true;
  LoadField();
}
