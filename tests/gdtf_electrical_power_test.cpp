#include "gdtf_description_reader.h"
#include "gdtf_mutation_audit.h"
#include "fixture_power_project_metadata.h"
#include <cassert>
#include <cmath>

using namespace gdtf;
static GdtfDescriptionSnapshot Read(const std::string &body) {
  return ReadGdtfDescription("<GDTF DataVersion=\"1.2\"><FixtureType>" + body +
      "<DMXModes><DMXMode Name=\"Default\"/></DMXModes></FixtureType></GDTF>");
}
static bool Has(const GdtfDescriptionSnapshot &snapshot, PowerDiagnosticCode code) {
  for (const auto &diagnostic : snapshot.electricalPower.diagnostics)
    if (diagnostic.code == code) return true;
  return false;
}
static std::string Consumer(const std::string &value) {
  return "<WiringObject ComponentType=\"Consumer\" ElectricalPayLoad=\"" + value + "\"/>";
}
static std::string Properties(const std::string &value) {
  return "<PhysicalDescriptions><Properties><PowerConsumption Connector=\"PowerCON\" Value=\"" + value +
      "\" PowerFactor=\"0.95\" VoltageLow=\"100\" VoltageHigh=\"240\" FrequencyLow=\"50\" FrequencyHigh=\"60\"/></Properties></PhysicalDescriptions>";
}
int main() {
  auto empty = Read("<PhysicalDescriptions><Properties><Weight Value=\"12\"/></Properties></PhysicalDescriptions>");
  assert(empty.weightKgPresent && empty.weightKg == 12);
  assert(!empty.powerConsumptionWPresent && Has(empty, PowerDiagnosticCode::MissingPlanningPower));
  auto source = Read("<Geometries><Geometry>" + Consumer("380") + "</Geometry></Geometries>");
  assert(source.powerConsumptionWPresent && source.powerConsumptionW == 380);
  assert(source.electricalPower.source == PlanningPowerSource::WiringConsumer);
  assert(Read(Properties("250")).powerConsumptionW == 250);
  for (const auto &value : {"380", "500"}) {
    auto both = Read(Properties(value) + "<Geometries><Geometry>" + Consumer("380") + "</Geometry></Geometries>");
    assert(both.powerConsumptionW == 380);
    assert(Has(both, PowerDiagnosticCode::ConflictingElectricalPower) == (std::string(value) == "500"));
    assert(both.electricalPower.properties.front().text == value);
  }
  auto multiple = Read(Properties("100") + "<Geometries><Geometry>" + Consumer("20") + Consumer("30") + "</Geometry></Geometries>");
  assert(!multiple.powerConsumptionWPresent && Has(multiple, PowerDiagnosticCode::AmbiguousConsumers));
  for (const auto &value : {"garbage", "-1", "nan", "inf"}) {
    auto invalid = Read("<Geometries><Geometry>" + Consumer(value) + "</Geometry></Geometries>");
    assert(!invalid.powerConsumptionWPresent && Has(invalid, PowerDiagnosticCode::InvalidElectricalValue));
  }
  assert(Read(Properties("0")).powerConsumptionWPresent);
  auto beam = Read("<Geometries><Beam PowerConsumption=\"40\"/></Geometries>");
  assert(beam.electricalPower.beamTotalW == 40 && !beam.powerConsumptionWPresent);
  auto beams = Read("<Geometries><Geometry><Beam PowerConsumption=\"40\"/><Beam PowerConsumption=\"20\"/></Geometry></Geometries>");
  assert(beams.electricalPower.beamTotalW == 60 && !beams.powerConsumptionWPresent);
  std::string instances = "<Geometries><Geometry Name=\"Fixture\">";
  for (int i = 0; i < 19; ++i) instances += "<GeometryReference Geometry=\"Emitter\"/>";
  instances += "</Geometry><Beam Name=\"Emitter\" PowerConsumption=\"40\"/></Geometries>";
  auto instanced = Read(instances);
  assert(instanced.electricalPower.beamTotalW == 760 && !instanced.powerConsumptionWPresent);
  assert(Has(Read(Properties("380") + instances), PowerDiagnosticCode::BeamExceedsInput));
  assert(!Has(Read(Properties("900") + instances), PowerDiagnosticCode::BeamExceedsInput));
  auto namedMode = Read(instances + "<DMXModes><DMXMode Name=\"Mode\" Geometry=\"Fixture\"/></DMXModes>");
  // The first DMXModes container from the body determines the common root.
  assert(namedMode.electricalPower.beamTotalW == 760);
  auto conflictingModes = Read(instances + "<DMXModes><DMXMode Name=\"A\" Geometry=\"Fixture\"/><DMXMode Name=\"B\" Geometry=\"Emitter\"/></DMXModes>");
  assert(!conflictingModes.electricalPower.beamTotalW && Has(conflictingModes, PowerDiagnosticCode::UnresolvedGeometry));
  auto partialBeam = Read("<Geometries><Geometry><Beam PowerConsumption=\"40\"/><Beam/></Geometry></Geometries>");
  assert(!partialBeam.electricalPower.beamTotalW);
  auto badBeam = Read("<Geometries><Geometry>" + Consumer("380") + "<Beam PowerConsumption=\"bad\"/></Geometry></Geometries>");
  assert(badBeam.powerConsumptionW == 380 && !badBeam.electricalPower.beamTotalW);
  auto instancedInput = Read("<Geometries><Geometry Name=\"Body\"><GeometryReference Geometry=\"Input\"/><GeometryReference Geometry=\"Input\"/></Geometry><Geometry Name=\"Input\">" + Consumer("100") + "</Geometry></Geometries>");
  assert(!instancedInput.powerConsumptionWPresent && Has(instancedInput, PowerDiagnosticCode::AmbiguousConsumers));
  auto cycle = Read("<Geometries><Geometry Name=\"Loop\"><GeometryReference Geometry=\"Loop\"/></Geometry></Geometries>");
  assert(!cycle.electricalPower.beamTotalW && Has(cycle, PowerDiagnosticCode::UnresolvedGeometry));
  auto broken = Read("<Geometries><GeometryReference Geometry=\"Missing\"/></Geometries>");
  assert(!broken.electricalPower.beamTotalW && Has(broken, PowerDiagnosticCode::UnresolvedGeometry));

  tinyxml2::XMLDocument doc;
  doc.Parse(("<FixtureType>" + Properties("100") + "</FixtureType>").c_str());
  auto *fixture = doc.FirstChildElement("FixtureType");
  assert(GdtfMutationAudit::ApplyPhysicalPropertiesWithAudit(fixture, doc, 12.0f, 380.0f, "Correct power"));
  auto *power = fixture->FirstChildElement("PhysicalDescriptions")->FirstChildElement("Properties")->FirstChildElement("PowerConsumption");
  assert(power->FloatAttribute("Value") == 380);
  assert(power->Attribute("Connector", "PowerCON") && power->Attribute("PowerFactor", "0.95"));
  assert(power->Attribute("VoltageLow", "100") && power->Attribute("FrequencyHigh", "60"));
  assert(fixture->FirstChildElement("Revisions")->FirstChildElement("Revision"));
  assert(GdtfMutationAudit::ApplyPhysicalPropertiesWithAudit(fixture, doc, {}, 390.0f, "Correct power"));
  assert(fixture->FirstChildElement("Revisions")->FirstChildElement("Revision")->NextSiblingElement("Revision"));
  doc.Clear(); doc.Parse("<FixtureType/>"); fixture = doc.FirstChildElement();
  assert(!GdtfMutationAudit::ApplyPhysicalPropertiesWithAudit(fixture, doc, 12.0f, 380.0f, "Unsafe"));
  assert(!fixture->FirstChildElement("PhysicalDescriptions") && !fixture->FirstChildElement("Revisions"));
  doc.Clear(); doc.Parse("<FixtureType><PhysicalDescriptions><Properties><PowerConsumption Value=\"100\"/></Properties></PhysicalDescriptions></FixtureType>");
  assert(!GdtfMutationAudit::ApplyPhysicalProperties(doc.FirstChildElement(), doc, {}, 380.0f));
  doc.Clear(); doc.Parse(("<FixtureType><Geometries><Geometry>" + Consumer("100") + "</Geometry></Geometries></FixtureType>").c_str());
  fixture = doc.FirstChildElement();
  assert(GdtfMutationAudit::ApplyPhysicalPropertiesWithAudit(fixture, doc, {}, 380.0f, "Correct input"));
  assert(fixture->FirstChildElement("Geometries")->FirstChildElement()->FirstChildElement()->FloatAttribute("ElectricalPayLoad") == 380);
  assert(!fixture->FirstChildElement("PhysicalDescriptions"));

  MvrScene scene;
  Fixture f; f.uuid = "fixture"; f.SetSourcePlanningPower(100.0f); f.SetPlanningPowerOverride(380.0f);
  f.SetSourcePlanningPower(200.0f); assert(f.powerConsumptionW == 380 && f.sourcePowerConsumptionW == 200);
  scene.fixtures.emplace(f.uuid, f);
  auto stored = fixture_power::SerializeOverrides(scene);
  scene.fixtures.at(f.uuid).planningPowerOverrideW.reset();
  scene.fixtures.at(f.uuid).SetSourcePlanningPower(200.0f);
  fixture_power::RestoreOverrides(scene, stored);
  assert(scene.fixtures.at(f.uuid).powerConsumptionW == 380);
  scene.fixtures.at(f.uuid).SetPlanningPowerOverride(0.0f);
  assert(scene.fixtures.at(f.uuid).HasPlanningPower());
}
