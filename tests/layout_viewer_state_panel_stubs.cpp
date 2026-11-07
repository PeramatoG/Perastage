#include "viewer2dpanel.h"
#include "viewer2drenderpanel.h"
#include <stdexcept>
// Only panel access is unavailable: tests run production state application and
// restoration with null panels against the real ConfigManager/history service.
Viewer2DViewState Viewer2DPanel::GetViewState() const { throw std::logic_error("Unexpected panel access"); }
void Viewer2DPanel::LoadViewFromConfig() { throw std::logic_error("Unexpected panel access"); }
void Viewer2DPanel::ApplyViewState(float,float,float,Viewer2DView,Viewer2DRenderMode) { throw std::logic_error("Unexpected panel access"); }
void Viewer2DPanel::UpdateScene(bool) { throw std::logic_error("Unexpected panel access"); }
void Viewer2DRenderPanel::ApplyConfig() { throw std::logic_error("Unexpected panel access"); }
