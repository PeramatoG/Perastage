#include <cassert>
#include <filesystem>
#include <fstream>
#include <wx/init.h>

#include "../core/configmanager.h"
#include "../core/gdtfdictionary.h"
#include "../core/projectutils.h"
#include "../models/fixture.h"
#include "../models/truss.h"
#include "../models/sceneobject.h"
#include "../models/layer.h"

int main() {
    wxInitializer initializer;
    assert(initializer.IsOk());

    auto &cfg = ConfigManager::Get();
    cfg.Reset();
    MvrScene &scene = cfg.GetScene();

    Layer layer; layer.uuid = "layer1"; layer.name = "Layer1"; layer.color = "#112233";
    scene.layers[layer.uuid] = layer;

    // Prepare dummy GDTF files
    std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "gdtf_roundtrip";
    std::filesystem::create_directories(tempDir);
    std::ofstream(tempDir / "orig.gdtf") << "orig";
    std::ofstream(tempDir / "dict.gdtf") << "dict";
    scene.basePath = tempDir.string();

    // Dictionary entry that should NOT be applied on load
    GdtfDictionary::Update("FixtureType", (tempDir / "dict.gdtf").string(), "");

    Fixture f; f.uuid = "fx1"; f.instanceName = "Fixture"; f.layer = layer.name; f.typeName = "FixtureType"; f.gdtfSpec = "orig.gdtf"; f.color = "#445566"; scene.fixtures[f.uuid] = f;
    Truss t; t.uuid = "tr1"; t.name = "Truss"; t.layer = layer.name; scene.trusses[t.uuid] = t;
    SceneObject o; o.uuid = "obj1"; o.name = "Object"; o.layer = layer.name; scene.sceneObjects[o.uuid] = o;

    std::filesystem::path temp = std::filesystem::temp_directory_path() / "roundtrip_test.pera";
    assert(cfg.SaveProject(temp.string()));

    cfg.Reset();

    assert(cfg.LoadProject(temp.string()));

    const auto &scene2 = cfg.GetScene();
    assert(scene2.fixtures.size() == 1);
    assert(scene2.trusses.size() == 1);
    assert(scene2.sceneObjects.size() == 1);
    assert(scene2.fixtures.at("fx1").instanceName == "Fixture");
    assert(scene2.trusses.at("tr1").name == "Truss");
    assert(scene2.sceneObjects.at("obj1").name == "Object");
    assert(scene2.fixtures.at("fx1").color == "#445566");
    assert(scene2.layers.at("layer1").color == "#112233");

    const auto &loaded = scene2.fixtures.at("fx1");
    assert(std::filesystem::path(loaded.gdtfSpec).filename() == "orig.gdtf");

    std::filesystem::remove(temp);
    std::filesystem::remove(ProjectUtils::GetDefaultLibraryPath("fixtures") + "/dict.gdtf");
    GdtfDictionary::Save({});
    std::filesystem::remove_all(tempDir);
    return 0;
}
