#include "fixture_label_preview.h"

void FixtureLabelPreview::SetActiveField(int field) {
  field = std::clamp(field, 0, 2);
  if (field == activeField_) return;
  CancelDrag();
  activeField_ = field;
  hover_ = false;
  Refresh(false);
}
void FixtureLabelPreview::SetDarkBackground(bool dark) {
  darkBackground_ = dark;
  Refresh(false);
}
bool FixtureLabelPreview::HitSelected(const wxPoint &pointer) const {
  return composition_.fields[activeField_] &&
      previewTransform_.Hit(composition_.fields[activeField_]->bounds,
                            {static_cast<double>(pointer.x), static_cast<double>(pointer.y)});
}
void FixtureLabelPreview::MouseDown(wxMouseEvent &event) {
  if (!HitSelected(event.GetPosition()) || !mouseCapture_.TryAcquire("label drag")) return;
  SetFocus();
  lockedScale_ = previewTransform_.scale;
  drag_.Begin({static_cast<double>(event.GetX()), static_cast<double>(event.GetY())},
              displayedStyles_[activeField_], previewTransform_.scale);
  if (dragCallback_)
    dragCallback_(label_preview::DragPhase::Begin, displayedStyles_[activeField_]);
  Refresh(false);
}
void FixtureLabelPreview::MouseMove(wxMouseEvent &event) {
  if (drag_.Active()) {
    const auto style = drag_.Move({static_cast<double>(event.GetX()),
                                   static_cast<double>(event.GetY())});
    if (style && dragCallback_)
      dragCallback_(label_preview::DragPhase::Move, *style);
    Refresh(false);
    return;
  }
  const bool hit = HitSelected(event.GetPosition());
  if (hover_ != hit) {
    hover_ = hit;
    SetCursor(hit ? wxCURSOR_HAND : wxCURSOR_ARROW);
    Refresh(false);
  }
}
void FixtureLabelPreview::MouseUp(wxMouseEvent &event) {
  if (!drag_.Active()) return;
  MouseMove(event);
  const bool changed = drag_.Changed();
  drag_.Finish();
  if (mouseCapture_.IsOwned()) mouseCapture_.Release("label drag end");
  if (dragCallback_)
    dragCallback_(changed ? label_preview::DragPhase::End : label_preview::DragPhase::Cancel,
                  displayedStyles_[activeField_]);
  Refresh(false);
}
void FixtureLabelPreview::CancelDrag() {
  if (!drag_.Active()) return;
  drag_.Finish();
  if (mouseCapture_.IsOwned()) mouseCapture_.Release("label drag cancel");
  if (dragCallback_)
    dragCallback_(label_preview::DragPhase::Cancel, displayedStyles_[activeField_]);
  Refresh(false);
}
void FixtureLabelPreview::CaptureLost(wxMouseCaptureLostEvent &) {
  mouseCapture_.AbandonOnLoss("label drag");
  CancelDrag();
}
