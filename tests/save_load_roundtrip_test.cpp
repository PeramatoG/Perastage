/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include <cassert>
#include <filesystem>
#include <fstream>
#include <wx/init.h>

#include "configmanager.h"
#include "gdtfdictionary.h"
#include "projectutils.h"
#include "fixture.h"
#include "truss.h"
#include "sceneobject.h"
#include "layer.h"
#include "support.h"

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

    Fixture f; f.uuid = "fx1"; f.instanceName = "Fixture"; f.layer = layer.name; f.typeName = "FixtureType"; f.gdtfSpec = "orig.gdtf"; f.color = "#445566"; f.fixtureIdText = "S101A"; f.fixtureIdNumeric = 101; f.fixtureId = 101; scene.fixtures[f.uuid] = f;
    Fixture f2; f2.uuid = "fx2"; f2.instanceName = "Fixture 2"; f2.layer = layer.name; f2.typeName = "FixtureType"; f2.gdtfSpec = "orig.gdtf"; f2.fixtureIdText = "S101B"; f2.fixtureIdNumeric = 101; f2.fixtureId = 101; scene.fixtures[f2.uuid] = f2;
    Truss t; t.uuid = "tr1"; t.name = "Truss"; t.layer = layer.name; scene.trusses[t.uuid] = t;
    SceneObject o; o.uuid = "obj1"; o.name = "Object"; o.layer = layer.name; scene.sceneObjects[o.uuid] = o;

    Support sManual; sManual.uuid = "sup-manual"; sManual.name = "Manual Hoist"; sManual.layer = layer.name;
    sManual.motorName = "ChainMaster D8+"; sManual.dummyPreset = "D8+ 1000kg";
    sManual.hoistDataSource = "Manual"; sManual.hoistFunction = "Audio";
    sManual.capacityKg = 700.0f; sManual.weightKg = 40.0f; scene.supports[sManual.uuid] = sManual;

    Support sInherited; sInherited.uuid = "sup-inherited"; sInherited.name = "Inherited Hoist"; sInherited.layer = layer.name;
    sInherited.motorName = "CM Lodestar"; sInherited.dummyPreset = "Lodestar 500kg";
    sInherited.hoistDataSource = "Inherited"; scene.supports[sInherited.uuid] = sInherited;

    std::filesystem::path temp = std::filesystem::temp_directory_path() / "roundtrip_test.pera";
    assert(cfg.SaveProject(temp.string()));

    cfg.Reset();

    assert(cfg.LoadProject(temp.string()));

    const auto &scene2 = cfg.GetScene();
    assert(scene2.fixtures.size() == 2);
    assert(scene2.trusses.size() == 1);
    assert(scene2.sceneObjects.size() == 1);
    assert(scene2.supports.size() == 2);
    assert(scene2.fixtures.at("fx1").instanceName == "Fixture");
    assert(scene2.trusses.at("tr1").name == "Truss");
    assert(scene2.sceneObjects.at("obj1").name == "Object");
    assert(scene2.fixtures.at("fx1").color == "#445566");
    assert(scene2.fixtures.at("fx1").fixtureIdText == "S101A");
    assert(scene2.fixtures.at("fx1").fixtureIdNumeric == 101);
    assert(scene2.fixtures.at("fx2").fixtureIdText == "S101B");
    assert(scene2.fixtures.at("fx2").fixtureIdNumeric == 101);
    assert(scene2.layers.at("layer1").color == "#112233");

    const auto &loadedManual = scene2.supports.at("sup-manual");
    assert(loadedManual.motorName == "ChainMaster D8+");
    assert(loadedManual.dummyPreset == "D8+ 1000kg");
    assert(loadedManual.hoistDataSource == "Manual");
    assert(loadedManual.capacityKg == 700.0f);
    assert(loadedManual.weightKg == 40.0f);

    const auto &loadedInherited = scene2.supports.at("sup-inherited");
    assert(loadedInherited.motorName == "CM Lodestar");
    assert(loadedInherited.dummyPreset == "Lodestar 500kg");
    assert(loadedInherited.hoistDataSource == "Inherited");

    const auto &loaded = scene2.fixtures.at("fx1");
    assert(std::filesystem::path(loaded.gdtfSpec).filename() == "orig.gdtf");

    std::filesystem::remove(temp);
    std::filesystem::remove(ProjectUtils::GetDefaultLibraryPath("fixtures") + "/dict.gdtf");
    GdtfDictionary::Save({});
    std::filesystem::remove_all(tempDir);
    return 0;
}
