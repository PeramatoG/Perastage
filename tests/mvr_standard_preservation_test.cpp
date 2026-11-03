/* This file is part of Perastage. */
#include "mvr_standard_preservation.h"
#include "mvr_standard_field_projection.h"
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
      "<Layer><Matrix>{1,0,0}{0,1,0}{0,0,1}{1,0,1000}</Matrix></Layer>",
      "<Fixture><Classing>10000000-0000-4000-8000-000000000001</Classing></Fixture>",
      "<Fixture multipatch='10000000-0000-4000-8000-000000000001'/>",
      "<Fixture><FixtureTypeId>12</FixtureTypeId></Fixture>",
      "<Fixture><Addresses><Address break='1'>1</Address><Address break='1'>513</Address></Addresses></Fixture>",
      "<Fixture><Addresses><Address>1</Address><Address break='0'>513</Address></Addresses></Fixture>",
      "<Fixture><Addresses><Network ipv4='192.168.1.1'/></Addresses></Fixture>",
      "<Fixture><Alignments><Alignment up='1,0,0'/></Alignments></Fixture>",
      "<Fixture><Protocols><Protocol type='sACN'/></Protocols></Fixture>",
      "<Fixture><CustomCommands><CustomCommand>Pan,f 50</CustomCommand></CustomCommands></Fixture>",
      "<Fixture><Overwrites><Overwrite universal='Filter'/></Overwrites></Fixture>",
      "<Fixture><Connections><Connection own='A' other='B' toObject='uuid'/></Connections></Fixture>",
      "<Fixture><Mappings><Mapping linkedDef='uuid'/></Mappings></Fixture>",
      "<Fixture><Gobo rotation='30'>gobo.png</Gobo></Fixture>",
      "<Fixture><ChildList><FocusPoint><Geometries/></FocusPoint></ChildList></Fixture>",
      "<SceneObject><GDTFSpec>speaker.gdtf</GDTFSpec></SceneObject>",
      "<SceneObject><Addresses/></SceneObject>",
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
  assert(Scan("<Layer><Matrix>{1,0,0}{0,1,0}{0,0,1}{0,0,1000}</Matrix></Layer>").preservationIssues.empty());
  assert(Scan("<Fixture><Addresses><Address break='7'>1</Address><Address>513</Address></Addresses></Fixture>").preservationIssues.empty());
  assert(Scan("<Fixture><Addresses><Network geometry='ethernet_1' ipv4='192.168.1.1'/></Addresses></Fixture>").preservationIssues.empty());
  for (const auto *type : {"SceneObject", "VideoScreen", "Projector"})
    assert(!Scan(std::string("<") + type + "><Addresses><Network geometry='ethernet_1'/></Addresses></" + type + ">").preservationIssues.empty());
  assert(Scan("<Truss><Addresses><Network geometry='ethernet_1'/></Addresses></Truss>").preservationIssues.empty());
  assert(Scan("<Support><Addresses><Network geometry='ethernet_1'/></Addresses></Support>").preservationIssues.empty());
  for (const auto *source : {"<Support><CustomId future='x'>3</CustomId></Support>",
      "<Support><CustomId>3</CustomId><CustomId>4</CustomId></Support>",
      "<Support><CustomIdType>2147483648</CustomIdType></Support>"})
    assert(!Scan(source).preservationIssues.empty());
  auto ordinary = Scan("<Fixture><Function>Key</Function><DMXInvertPan>true</DMXInvertPan><Addresses><Address break='0'>1025</Address></Addresses></Fixture>");
  assert(ordinary.preservationIssues.empty());
  // Whitespace-only optional containers have the same empty standard meaning.
  auto emptyContainers = Scan("<Fixture><Protocols>&#x20;&#xA;&#x9;</Protocols><Alignments> \r </Alignments>"
      "<CustomCommands> </CustomCommands><Overwrites> </Overwrites>"
      "<Connections> </Connections><Mappings> </Mappings></Fixture>");
  assert(emptyContainers.preservationIssues.empty());
  assert(mvr::CheckStandardPreservation(emptyContainers).empty());
  for (const auto *payload : {"text", "<Protocol/>", "<![CDATA[meaningful]]>"}) {
    auto nonempty = Scan(std::string("<Fixture><Protocols>") + payload + "</Protocols></Fixture>");
    assert(!nonempty.preservationIssues.empty());
  }
  assert(!Scan("<Fixture><Protocols unknown='value'> </Protocols></Fixture>").preservationIssues.empty());
  // Valid retained fields are serialized rather than treated as loss blockers.
  auto retained = Scan("<GeneralSceneDescription><Scene><Layers><Layer><ChildList>"
      "<Fixture uuid='50000000-0000-4000-8000-000000000001'><CastShadow>true</CastShadow>"
      "<Protocols><Protocol type='sACN'/></Protocols><CustomCommands><CustomCommand>Pan,f 50</CustomCommand></CustomCommands>"
      "<Connections><Connection own='A' other='B' toObject='50000000-0000-4000-8000-000000000001'/></Connections>"
      "</Fixture></ChildList></Layer></Layers></Scene></GeneralSceneDescription>");
  assert(retained.preservationIssues.size() == 4);
  assert(mvr::CheckStandardPreservation(retained).empty());
  mvr::ReconcileStandardSourceFields(retained, [](const std::string &uuid) {
    return uuid == "50000000-0000-4000-8000-000000000001" ? "50000000-0000-4000-8000-000000000002" : uuid;
  }, {});
  assert(retained.preservationIssues[0].objectUuid == "50000000-0000-4000-8000-000000000002");
  assert(retained.preservationIssues[3].originalInformation.find("50000000-0000-4000-8000-000000000002") != std::string::npos);
  tinyxml2::XMLDocument output;
  output.Parse("<GeneralSceneDescription><Scene><Layers><Layer><ChildList><Fixture uuid='50000000-0000-4000-8000-000000000002'/></ChildList></Layer></Layers></Scene></GeneralSceneDescription>");
  assert(mvr::ProjectStandardSourceFields(output, retained, {}).diagnostics.empty());
  auto *fixture = output.RootElement()->FirstChildElement("Scene")->FirstChildElement("Layers")->FirstChildElement("Layer")->FirstChildElement("ChildList")->FirstChildElement("Fixture");
  assert(fixture->FirstChildElement("CastShadow") && fixture->FirstChildElement("Connections"));
  auto aliases = Scan("<GeneralSceneDescription><Fixture uuid='5AAAAAAA-0000-4000-8000-000000000001'>"
      "<Connections><Connection own='A' other='B' toObject='5AAAAAAA-0000-4000-8000-000000000001'/></Connections>"
      "</Fixture></GeneralSceneDescription>");
  mvr::ReconcileStandardSourceFields(aliases, [](const std::string &uuid) {
    return uuid == "5aaaaaaa-0000-4000-8000-000000000001" ? "5aaaaaaa-0000-4000-8000-000000000002" : uuid;
  }, {});
  assert(aliases.preservationIssues[0].objectUuid == "5aaaaaaa-0000-4000-8000-000000000002");
  assert(aliases.preservationIssues[0].originalInformation.find("5aaaaaaa-0000-4000-8000-000000000002") != std::string::npos);
  assert(aliases.preservationIssues[0].originalInformation.find("5AAAAAAA") == std::string::npos);
  safe.versionMinor = 7;
  assert(!mvr::CheckStandardPreservation(safe).empty());
  safe.Clear();
  assert(mvr::CheckStandardPreservation(safe).empty());
  tinyxml2::XMLDocument doc;
  assert(doc.Parse("<Scene><Layers><Layer><ChildList><FocusPoint uuid='30000000-0000-4000-8000-000000000001'/><Fixture uuid='fixture'><Focus>missing</Focus></Fixture></ChildList></Layer></Layers></Scene>") == tinyxml2::XML_SUCCESS);
  assert(mvr::CheckSerializedFocus(*doc.RootElement()).size() == 2);
}
