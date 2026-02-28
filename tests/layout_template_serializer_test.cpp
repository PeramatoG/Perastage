#include "LayoutTemplateSerializer.h"

#include <cassert>

namespace {
using json = nlohmann::json;

layouts::LayoutDefinition BuildFullLayoutDefinition() {
  layouts::LayoutDefinition layout;
  layout.name = "Complete Layout";
  layout.pageSetup.pageSize = print::PageSize::A3;
  layout.pageSetup.landscape = true;

  layouts::Layout2DViewDefinition view;
  view.id = 11;
  view.zIndex = 4;
  view.frame = {10, 20, 300, 200};
  view.camera.offsetPixelsX = 12.5f;
  view.camera.offsetPixelsY = -2.0f;
  view.camera.zoom = 2.5f;
  view.camera.viewportWidth = 1920;
  view.camera.viewportHeight = 1080;
  view.camera.view = 2;
  view.renderOptions.renderMode = 1;
  view.renderOptions.darkMode = false;
  view.renderOptions.forceBottomViewForTopFixtures = true;
  view.renderOptions.showGrid = false;
  view.renderOptions.gridStyle = 3;
  view.renderOptions.gridColorR = 0.4f;
  view.renderOptions.gridColorG = 0.5f;
  view.renderOptions.gridColorB = 0.6f;
  view.renderOptions.gridDrawAbove = true;
  view.renderOptions.showLabelName = {true, false, true};
  view.renderOptions.showLabelId = {false, true, false};
  view.renderOptions.showLabelDmx = {true, true, false};
  view.renderOptions.labelFontSizeName = 9.0f;
  view.renderOptions.labelFontSizeId = 8.0f;
  view.renderOptions.labelFontSizeDmx = 7.0f;
  view.renderOptions.labelOffsetDistance = {1.0f, 1.5f, 2.0f};
  view.renderOptions.labelOffsetAngle = {90.0f, 180.0f, 270.0f};
  view.layers.hiddenLayers = {"Layer A", "Layer B"};
  layout.view2dViews.push_back(view);

  layouts::LayoutLegendDefinition legend;
  legend.id = 12;
  legend.zIndex = 5;
  legend.frame = {2, 3, 80, 120};
  layout.legendViews.push_back(legend);

  layouts::LayoutEventTableDefinition table;
  table.id = 13;
  table.zIndex = 6;
  table.frame = {6, 7, 400, 150};
  table.fields = {"Venue", "Location", "Date", "Stage", "Version", "Design", "Mail"};
  layout.eventTables.push_back(table);

  layouts::LayoutTextDefinition text;
  text.id = 14;
  text.zIndex = 7;
  text.frame = {9, 9, 200, 80};
  text.text = "Plain";
  text.richText = "<b>Rich</b>";
  text.solidBackground = false;
  text.drawFrame = false;
  layout.textViews.push_back(text);

  layouts::LayoutImageDefinition image;
  image.id = 15;
  image.zIndex = 8;
  image.frame = {1, 2, 100, 60};
  image.imagePath = "missing_image_for_serializer_test.png";
  image.aspectRatio = 1.8f;
  layout.imageViews.push_back(image);

  return layout;
}

void TestFullRoundTrip() {
  const layouts::LayoutDefinition original = BuildFullLayoutDefinition();
  const json document = layouts::ToTemplateDocument({original});

  std::vector<layouts::LayoutDefinition> imported;
  layouts::LayoutTemplateImportReport report;
  std::string error;
  const bool ok = layouts::FromTemplateDocument(document, imported, &report, &error);

  assert(ok);
  assert(error.empty());
  assert(imported.size() == 1);

  const layouts::LayoutDefinition &layout = imported.front();
  assert(layout.name == original.name);
  assert(layout.pageSetup.pageSize == print::PageSize::A3);
  assert(layout.pageSetup.landscape == original.pageSetup.landscape);
  assert(layout.view2dViews.size() == 1);
  assert(layout.legendViews.size() == 1);
  assert(layout.eventTables.size() == 1);
  assert(layout.textViews.size() == 1);
  assert(layout.imageViews.size() == 1);

  assert(layout.view2dViews.front().camera.view == 2);
  assert(layout.legendViews.front().id == 12);
  assert(layout.eventTables.front().fields[0] == "Venue");
  assert(layout.textViews.front().richText == "<b>Rich</b>");
  assert(layout.imageViews.front().aspectRatio == original.imageViews.front().aspectRatio);

  assert(report.layouts.imported == 1);
  assert(report.layouts.skipped == 0);
  assert(report.view2dViews.imported == 1);
  assert(report.legendViews.imported == 1);
  assert(report.eventTables.imported == 1);
  assert(report.textViews.imported == 1);
  assert(report.imageViews.imported == 1);
}

void TestImportWithMissingFieldsUsesDefaults() {
  json input = {{"schemaVersion", 1},
                {"layouts", json::array({{{"name", "Missing fields"}}})}};

  std::vector<layouts::LayoutDefinition> imported;
  std::string error;
  const bool ok = layouts::FromTemplateDocument(input, imported, &error);

  assert(ok);
  assert(error.empty());
  assert(imported.size() == 1);
  const auto &layout = imported.front();
  assert(layout.pageSetup.pageSize == print::PageSize::A4);
  assert(!layout.pageSetup.landscape);
  assert(layout.view2dViews.empty());
  assert(layout.legendViews.empty());
  assert(layout.eventTables.empty());
  assert(layout.textViews.empty());
  assert(layout.imageViews.empty());
}

void TestImportWithCorruptTypes() {
  json input = {
      {"schemaVersion", 1},
      {"layouts", json::array({
                      {{"name", "Valid"},
                       {"view2dViews", "not_an_array"},
                       {"legendViews", json::array({17, {"id", 1}})},
                       {"eventTables", json::array({nullptr})},
                       {"textViews", json::array({{"text", 15}})},
                       {"imageViews", json::array({{"path", 3.14}})}} ,
                      42,
                  })}};

  std::vector<layouts::LayoutDefinition> imported;
  layouts::LayoutTemplateImportReport report;
  std::string error;
  const bool ok = layouts::FromTemplateDocument(input, imported, &report, &error);

  assert(ok);
  assert(error.empty());
  assert(imported.size() == 1);
  assert(report.layouts.imported == 1);
  assert(report.layouts.skipped == 1);
  assert(!report.warnings.empty());
  assert(!report.errors.empty());
}

void TestUnknownSchemaVersionIsAccepted() {
  json input = {{"schemaVersion", 999},
                {"layouts", json::array({{{"name", "Future schema"}}})}};

  std::vector<layouts::LayoutDefinition> imported;
  std::string error;
  const bool ok = layouts::FromTemplateDocument(input, imported, &error);

  assert(ok);
  assert(error.empty());
  assert(imported.size() == 1);
  assert(imported.front().name == "Future schema");
}

} // namespace

int main() {
  TestFullRoundTrip();
  TestImportWithMissingFieldsUsesDefaults();
  TestImportWithCorruptTypes();
  TestUnknownSchemaVersionIsAccepted();
  return 0;
}
