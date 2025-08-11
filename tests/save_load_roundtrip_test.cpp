#include <cassert>
#include <filesystem>
#include <wx/init.h>

#include "../core/configmanager.h"
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

    Layer layer; layer.uuid = "layer1"; layer.name = "Layer1";
    scene.layers[layer.uuid] = layer;

    Fixture f; f.uuid = "fx1"; f.instanceName = "Fixture"; f.layer = layer.name; scene.fixtures[f.uuid] = f;
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

    std::filesystem::remove(temp);
    return 0;
}
