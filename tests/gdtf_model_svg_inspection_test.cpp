#include "symbols/gdtf_model_svg_inspection.h"
#include "symbols/PerastageSvgSymbol.h"
#include "symbols/standard_gdtf_completion.h"
#include "gdtf_canonicalizer.h"
#include "gdtf_thumbnail.h"
#include "gdtf_test_fixture_builder.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <tinyxml2.h>

namespace fs = std::filesystem;

std::string Bytes(const fs::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), {}};
}

int main() {
  const fs::path directory = fs::temp_directory_path() / "perastage-model-svg-inspection";
  fs::remove_all(directory);
  fs::create_directories(directory);
  const auto seed = directory / "seed.gdtf";
  const auto fixture = directory / "fixture.gdtf";
  const std::string authored =
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\">"
      "<path d=\"M10 10 C90 10 90 90 10 90 Z\" fill=\"red\"/></svg>";
  PerastageSvgSymbolData internal;
  assert(!ParseFixtureSymbolSvg(authored, internal));
  auto builder = tests::gdtf::BuildMinimalValidFixture().WithModelResource("seed");
  for (const auto *file : {"base", "yoke", "head", "pixel1"})
    for (const auto &view : symbols::GdtfModelSvgViews("1.2"))
      builder.WithArchiveEntry(view.directory + "/" + file + ".svg", authored);
  builder.WithArchiveEntry("models/svg/invalid.svg", "<svg")
      .WithArchiveEntry("models/svg/legacy.svg",
          "<svg data-perastage-symbol-version=\"1\" viewBox=\"0 0 10 10\"><polygon points=\"0,0 10,0 10,10\"/></svg>")
      .WithArchiveEntry("thumb.svg", authored)
      .WithArchiveEntry("thumb.png", "canonical PNG")
      .WithArchiveEntry("thumbnails/thumb.png", "compatibility PNG")
      .WithArchiveEntry("thumb.jpg", "compatibility JPEG")
      .WriteArchive(seed);
  const auto rewrite = GdtfCanonicalizer::RewriteArchiveDescription(
      seed, fixture, [](tinyxml2::XMLDocument &document) {
        auto *root = document.FirstChildElement("GDTF");
        auto *type = root->FirstChildElement("FixtureType");
        type->SetAttribute("Thumbnail", "thumb");
        auto *models = type->FirstChildElement("Models");
        models->DeleteChildren();
        for (const auto &[name, file] : {
             std::pair{"Base", "base"}, {"Yoke", "yoke"}, {"Head", "head"},
             {"Pixel", "pixel1"}, {"Primitive", ""}, {"Invalid", "invalid"},
             {"Legacy", "legacy"}}) {
          auto *model = document.NewElement("Model");
          model->SetAttribute("Name", name);
          if (*file)
            model->SetAttribute("File", file);
          model->SetAttribute("Length", 0.2);
          model->SetAttribute("Width", 0.3);
          model->SetAttribute("Height", 0.4);
          model->SetAttribute("PrimitiveType", "Cube");
          model->SetAttribute("SVGOffsetX", 7.25);
          model->SetAttribute("SVGFrontOffsetY", -2.5);
          models->InsertEndChild(model);
        }
        auto *geometries = type->FirstChildElement("Geometries");
        geometries->DeleteChildren();
        auto *base = document.NewElement("Geometry");
        base->SetAttribute("Name", "Root");
        base->SetAttribute("Model", "Base");
        geometries->InsertEndChild(base);
        for (const auto *name : {"PixelA", "PixelB"}) {
          auto *pixel = document.NewElement("Geometry");
          pixel->SetAttribute("Name", name);
          pixel->SetAttribute("Model", "Pixel");
          base->InsertEndChild(pixel);
        }
        return true;
      });
  assert(rewrite.success);
  const auto original = Bytes(fixture);
  const auto preserved = directory / "preserved.gdtf";
  const auto canonical = GdtfCanonicalizer::CanonicalizeArchive(fixture, preserved, {});
  assert(canonical.success && !canonical.changed && Bytes(preserved) == original);
  const auto catalog = symbols::InspectGdtfModelSvgs(fixture);
  assert(catalog.success && catalog.dataVersion == "1.2" && catalog.models.size() == 7);
  const std::vector<std::string> names = {"Base", "Yoke", "Head", "Pixel"};
  for (size_t index = 0; index < names.size(); ++index) {
    const auto &model = catalog.models[index];
    assert(model.name == names[index] && model.views.size() == 3);
    assert(model.lengthMeters == 0.2 && model.primitiveType == "Cube");
    assert(!model.ambiguousFile);
    for (size_t view = 0; view < model.views.size(); ++view) {
      const auto &resource = model.views[view];
      assert(resource.view != SymbolViewKind::Bottom);
      assert(resource.present && resource.state == symbols::ModelSvgResourceState::Present);
      assert(resource.archivePath == symbols::GdtfModelSvgViews("1.2")[view].directory +
          "/" + model.file + ".svg");
    }
    assert(model.views[0].offsetX == 7.25 && model.views[1].offsetY == -2.5);
  }
  assert(catalog.models[3].geometryReferences.size() == 2);
  assert(catalog.models[3].geometryReferences[0] != catalog.models[3].geometryReferences[1]);
  assert(catalog.models[4].file.empty() && !catalog.models[4].diagnostics.empty());
  for (const auto &view : catalog.models[4].views)
    assert(!view.present && view.archivePath.empty());
  assert(catalog.models[5].views[0].present);
  assert(catalog.models[5].views[0].state == symbols::ModelSvgResourceState::Malformed);
  assert(catalog.models[5].views[1].state == symbols::ModelSvgResourceState::Missing);
  assert(catalog.models[6].views[0].present && catalog.models[6].views[0].legacyPerastage);
  assert(!catalog.models[6].views[0].diagnostic.empty());
  const auto thumbnails = gdtf::InspectGdtfThumbnails(fixture);
  assert(thumbnails.size() == 2);
  assert(thumbnails[0].archivePath == "thumb.svg" && thumbnails[0].svg);
  assert(!thumbnails[0].compatibilityFallback && !thumbnails[1].compatibilityFallback);
  assert(thumbnails[1].archivePath == "thumb.png");
  const auto compatibility = directory / "compatibility.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithFixtureTypeExtensionAttribute("Thumbnail", "thumb")
      .WithArchiveEntry("thumbnails/thumb.png", "legacy PNG").WriteArchive(compatibility);
  const auto compatibilityThumbnails = gdtf::InspectGdtfThumbnails(compatibility);
  assert(compatibilityThumbnails.size() == 1 && compatibilityThumbnails[0].compatibilityFallback);
  assert(Bytes(fixture) == original);
  assert(symbols::GdtfModelSvgViews("1.1").size() == 1);
  assert(symbols::GdtfModelSvgViews("1.3").empty());

  const auto duplicate = directory / "duplicate.gdtf";
  assert(GdtfCanonicalizer::RewriteArchiveDescription(fixture, duplicate,
      [](tinyxml2::XMLDocument &document) {
        auto *models = document.FirstChildElement("GDTF")->FirstChildElement("FixtureType")
            ->FirstChildElement("Models");
        models->InsertEndChild(models->FirstChildElement("Model")->DeepClone(&document));
        return true;
      }).success);
  const auto duplicated = symbols::InspectGdtfModelSvgs(duplicate);
  assert(duplicated.success && duplicated.models.front().ambiguousFile);
  assert(duplicated.models.back().ambiguousFile);
  const auto duplicateBytes = Bytes(duplicate);
  fixture_gdtf::PreparedDerivative prepared;
  std::string error;
  assert(fixture_gdtf::PrepareOwnedDerivative(duplicate, directory / "output.gdtf",
      "output.gdtf", prepared, error));
  const auto rejected = symbols::CompleteStandardGdtfViews(duplicate, prepared, {},
      gdtf::MutationPolicy::CompleteAndImprove, "base");
  assert(!rejected.success && rejected.errorMessage.find("multiple") != std::string::npos);
  assert(!fs::exists(prepared.workingPath) && Bytes(duplicate) == duplicateBytes);
  for (const auto *selector : {"", "Main", "nonexistent"}) {
    assert(fixture_gdtf::PrepareOwnedDerivative(fixture, directory / "output.gdtf",
        "output.gdtf", prepared, error));
    assert(!symbols::CompleteStandardGdtfViews(fixture, prepared, {},
        gdtf::MutationPolicy::CompleteAndImprove, selector).success);
    assert(!fs::exists(prepared.workingPath) && Bytes(fixture) == original);
  }
  fs::remove_all(directory);
}
