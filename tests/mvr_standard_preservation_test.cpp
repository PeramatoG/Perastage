/* This file is part of Perastage. */
#include "mvr_standard_preservation.h"
#include <tinyxml2.h>
#include <cassert>

static MvrScene Scan(const std::string &xml) {
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  MvrScene scene;
  mvr::RecordStandardPreservationRisks(*doc.RootElement(), scene);
  return scene;
}

int main() {
  for (const auto *xml : {
      "<AUXData><Class uuid='invalid'/></AUXData>",
      "<AUXData><MappingDefinition><SizeX>1920</SizeX></MappingDefinition></AUXData>",
      "<Layer><Matrix>{1,0,0}{0,1,0}{0,0,1}{0,0,1000}</Matrix></Layer>",
      "<Fixture><Classing>10000000-0000-4000-8000-000000000001</Classing></Fixture>",
      "<Fixture multipatch='10000000-0000-4000-8000-000000000001'/>",
      "<Fixture><FixtureTypeId>12</FixtureTypeId></Fixture>",
      "<Fixture><Addresses><Address break='1'>1</Address></Addresses></Fixture>",
      "<Fixture><Addresses><Address>1</Address><Address break='1'>513</Address></Addresses></Fixture>",
      "<Fixture><Addresses><Network geometry='ethernet_1' ipv4='192.168.1.1'/></Addresses></Fixture>",
      "<Fixture><Alignments><Alignment up='1,0,0'/></Alignments></Fixture>",
      "<Fixture><Protocols><Protocol type='sACN'/></Protocols></Fixture>",
      "<Fixture><CustomCommands><CustomCommand>Pan,f 50</CustomCommand></CustomCommands></Fixture>",
      "<Fixture><Overwrites><Overwrite universal='Filter'/></Overwrites></Fixture>",
      "<Fixture><Connections><Connection own='A' other='B' toObject='uuid'/></Connections></Fixture>",
      "<Fixture><Mappings><Mapping linkedDef='uuid'/></Mappings></Fixture>",
      "<Fixture><Gobo rotation='30'>gobo.png</Gobo></Fixture>",
      "<Fixture><ChildList><FocusPoint><Geometries/></FocusPoint></ChildList></Fixture>",
      "<SceneObject><GDTFSpec>speaker.gdtf</GDTFSpec></SceneObject>",
      "<FocusPoint><Geometries><Symbol uuid='uuid' symdef='def'/></Geometries></FocusPoint>",
      "<FocusPoint/>",
      "<VideoScreen><Sources><Source type='File'>movie.mov</Source></Sources></VideoScreen>",
      "<Projector><Projections/></Projector>",
      "<Symdef><ChildList><Symbol symdef='uuid'/></ChildList></Symdef>"}) {
    const auto scene = Scan(xml);
    assert(!scene.preservationIssues.empty());
    const auto diagnostics = mvr::CheckStandardPreservation(scene);
    assert(!diagnostics.empty());
    assert(diagnostics.front().severity == MvrExportDiagnosticSeverity::Error);
    assert(diagnostics.front().impact == MvrExportDiagnosticImpact::ExportFailed);
    assert(!scene.preservationIssues.front().originalInformation.empty());
    assert(!scene.preservationIssues.front().recommendedAction.empty());
  }
  auto safe = Scan("<GeneralSceneDescription><UserData><Data provider='Vendor'><Fixture><Alignments/></Fixture></Data></UserData><Scene><Layers/></Scene></GeneralSceneDescription>");
  assert(safe.preservationIssues.empty());
  assert(mvr::CheckStandardPreservation(safe).empty());
  auto ordinary = Scan("<Fixture><Function>Key</Function><DMXInvertPan>true</DMXInvertPan><Addresses><Address break='0'>1025</Address></Addresses></Fixture>");
  assert(ordinary.preservationIssues.empty());
  safe.versionMinor = 7;
  assert(!mvr::CheckStandardPreservation(safe).empty());
  safe.Clear();
  assert(mvr::CheckStandardPreservation(safe).empty());
  tinyxml2::XMLDocument doc;
  assert(doc.Parse("<Scene><Layers><Layer><ChildList><FocusPoint uuid='30000000-0000-4000-8000-000000000001'/><Fixture uuid='fixture'><Focus>missing</Focus></Fixture></ChildList></Layer></Layers></Scene>") == tinyxml2::XML_SUCCESS);
  assert(mvr::CheckSerializedFocus(*doc.RootElement()).size() == 2);
}
