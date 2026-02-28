#include "LayoutTemplateSerializer.h"

#include <cassert>

int main() {
  using json = nlohmann::json;

  json view = {
      {"id", 1},
      {"frame", {{"x", 1}, {"y", 2}, {"width", -3}, {"height", 4}}},
      {"camera", {{"zoom", 0.0f}, {"view", 9}}},
      {"renderOptions", {{"showLabelName", json::array({true, false})}}},
      {"layers", {{"hiddenLayers", json::array({"Layer A", "", "Layer A", 7})}}}};

  json image = {{"id", 2},
                {"path", "definitely_missing_image.png"},
                {"aspectRatio", 9999.0f}};

  json layout = {{"name", "Portable"},
                 {"pageSetup", {{"pageSize", "A3"}, {"landscape", true}}},
                 {"view2dViews", json::array({view})},
                 {"imageViews", json::array({image})}};

  json input = {{"schemaVersion", 1}, {"layouts", json::array({layout, layout})}};

  std::vector<layouts::LayoutDefinition> layoutsOut;
  layouts::LayoutTemplateImportReport report;
  std::string error;
  const bool ok = layouts::FromTemplateDocument(input, layoutsOut, &report, &error);
  assert(ok);
  assert(error.empty());

  assert(layoutsOut.size() == 1);
  const auto &loaded = layoutsOut.front();
  assert(loaded.pageSetup.pageSize == print::PageSize::A3);
  assert(loaded.pageSetup.landscape);

  assert(loaded.view2dViews.size() == 1);
  const auto &loadedView = loaded.view2dViews.front();
  assert(loadedView.frame.width == 0);
  assert(loadedView.camera.zoom >= 0.01f);
  assert(loadedView.camera.view == 2);
  assert(loadedView.renderOptions.showLabelName[0]);
  assert(!loadedView.renderOptions.showLabelName[1]);
  assert(loadedView.renderOptions.showLabelName[2]);
  assert(loadedView.layers.hiddenLayers.size() == 1);
  assert(loadedView.layers.hiddenLayers[0] == "Layer A");

  assert(loaded.imageViews.size() == 1);
  assert(loaded.imageViews.front().aspectRatio == 100.0f);

  assert(report.layouts.imported == 1);
  assert(report.layouts.skipped == 1);
  assert(!report.warnings.empty());

  const json output = layouts::ToTemplateDocument(layoutsOut);
  assert(output.at("resources").at("imagePathPolicy") == "preserve_original_path");
  assert(output.at("layouts").at(0).at("pageSetup").is_object());

  return 0;
}
