#include "inspection/gdtf_inspector_presentation.h"

#include <cassert>
#include <string>

namespace {

// Returns one projected overview row by its stable field identity.
const gui::inspection::GdtfOverviewRow &FindOverview(
    const gui::inspection::GdtfOverviewPresentation &presentation,
    gui::inspection::GdtfOverviewField field) {
  for (const auto &row : presentation.rows) {
    if (row.field == field)
      return row;
  }
  assert(false);
  return presentation.rows.front();
}

// Verifies complete metadata projection and authored-value availability.
void CheckOverview() {
  gdtf::GdtfDescriptionSnapshot source;
  source.dataVersion = "1.2";
  source.fixtureTypeName = "Fixture";
  source.fixtureTypeId = "id";
  source.manufacturer = "Maker";
  source.shortName = "Short";
  source.longName = "Long";
  source.description = "Description";
  source.thumbnail = "thumb";
  source.createDate = "2026-01-02";
  source.revision = "rev";
  source.weightKgPresent = true;
  source.weightKg = 0.0f;
  source.modelColorHex = "#123456";
  source.trussCrossSectionType = "TrussFramework";
  source.trussCrossSection = "100,200";
  source.revisions.push_back({"Initial", "2026-01-02", "user-7", "Tester"});

  const auto result = gui::inspection::BuildGdtfOverviewPresentation(source);
  assert(FindOverview(result, gui::inspection::GdtfOverviewField::DataVersion)
             .value.value == "1.2");
  assert(FindOverview(
             result, gui::inspection::GdtfOverviewField::FixtureTypeName)
             .value.value == "Fixture");
  assert(FindOverview(result, gui::inspection::GdtfOverviewField::FixtureTypeId)
             .value.value == "id");
  assert(FindOverview(result, gui::inspection::GdtfOverviewField::Manufacturer)
             .value.value == "Maker");
  assert(FindOverview(result, gui::inspection::GdtfOverviewField::ShortName)
             .value.value == "Short");
  assert(FindOverview(result, gui::inspection::GdtfOverviewField::LongName)
             .value.value == "Long");
  assert(FindOverview(result, gui::inspection::GdtfOverviewField::Description)
             .value.value == "Description");
  assert(FindOverview(result, gui::inspection::GdtfOverviewField::Thumbnail)
             .value.value == "thumb");
  assert(FindOverview(result, gui::inspection::GdtfOverviewField::CreationDate)
             .value.value == "2026-01-02");
  assert(FindOverview(result, gui::inspection::GdtfOverviewField::Revision)
             .value.value == "rev");
  assert(FindOverview(result, gui::inspection::GdtfOverviewField::Weight)
             .value.available);
  assert(FindOverview(result, gui::inspection::GdtfOverviewField::Weight)
             .value.value == "0 kg");
  assert(!FindOverview(
              result, gui::inspection::GdtfOverviewField::PowerConsumption)
              .value.available);
  assert(FindOverview(result, gui::inspection::GdtfOverviewField::ModelColor)
             .value.value == "#123456");
  assert(FindOverview(
             result,
             gui::inspection::GdtfOverviewField::TrussCrossSectionType)
             .value.value == "TrussFramework");
  assert(FindOverview(
             result, gui::inspection::GdtfOverviewField::TrussCrossSection)
             .value.value == "100,200");
  assert(result.revisions.size() == 1);
  assert(result.revisions.front().text.value == "Initial");
  assert(result.revisions.front().date.value == "2026-01-02");
  assert(result.revisions.front().userId.value == "user-7");
  assert(result.revisions.front().modifiedBy.value == "Tester");

  source.powerConsumptionWPresent = true;
  source.powerConsumptionW = 250.0f;
  const auto withPower =
      gui::inspection::BuildGdtfOverviewPresentation(source);
  assert(FindOverview(
             withPower,
             gui::inspection::GdtfOverviewField::PowerConsumption)
             .value.value == "250 W");
}

// Verifies mode projection delegates hierarchy and detail semantics.
void CheckMode() {
  gdtf::GdtfDmxModeNode mode;
  mode.description = "Mode description";
  mode.geometry = "Head";
  mode.calculatedFootprint = 2;
  gdtf::GdtfDmxChannelNode channel;
  channel.id = "channel";
  channel.geometry = "Head";
  channel.rawOffset = "1,2";
  channel.resolution = 2;
  gdtf::GdtfLogicalChannelNode logical;
  logical.id = "logical";
  logical.attribute = "Dimmer";
  logical.attributeInfo.physicalUnit = "Percent";
  gdtf::GdtfChannelFunctionNode function;
  function.id = "function";
  function.name = "Dim";
  function.modeMaster = "Control";
  function.effectiveDmxRange = gdtf::GdtfDmxRange{0, 255};
  function.effectivePhysicalRange = {"0", "100",
                                     gdtf::GdtfValueOrigin::Explicit,
                                     gdtf::GdtfValueOrigin::Explicit, true};
  function.physicalUnit = "Percent";
  gdtf::GdtfChannelSetNode set;
  set.id = "set";
  set.name = "Low";
  set.subChannelSets.push_back({"subset", "Fine", "0", "1", "Percent"});
  function.channelSets.push_back(set);
  logical.channelFunctions.push_back(function);
  channel.logicalChannels.push_back(logical);
  mode.channels.push_back(channel);

  const auto result =
      gui::inspection::BuildGdtfInspectorModePresentation(mode);
  assert(result.description.value == "Mode description");
  assert(result.geometry.value == "Head");
  assert(result.calculatedFootprint == 2);
  assert(result.nodes.size() == 5);
  assert(result.nodes[2].dmxRange == "0 -> 255");
  assert(result.nodes[2].physicalRange == "0 -> 100");
  bool foundModeMaster = false;
  for (const auto &detail : result.nodes[2].details)
    foundModeMaster |= detail.key == "ModeMaster" && detail.value == "Control";
  assert(foundModeMaster);
}

// Verifies all lightweight wheel resource identities remain visible.
void CheckWheels() {
  gdtf::GdtfWheelCatalog catalog;
  gdtf::GdtfCatalogWheelInfo wheel;
  wheel.name = "Graphic";
  wheel.type = "Graphic";
  gdtf::GdtfCatalogWheelSlotInfo slot;
  slot.index = 2;
  slot.name = "Pattern";
  slot.rawColor = "0.3,0.3,1";
  slot.rawFilter = "Filter.Blue";
  slot.mediaFileName = "pattern";
  slot.resolvedResourcePath = "wheels/pattern.png";
  slot.graphicWheelReference = "Graphic.Ref";
  slot.graphicWheelResource = "graphics/pattern.png";
  wheel.slots.push_back(slot);
  catalog.wheels.push_back(wheel);
  catalog.filters.push_back({"filter", "Blue", "0.1,0.2,1"});

  const auto result = gui::inspection::BuildGdtfWheelsPresentation(catalog);
  assert(result.wheels.size() == 1);
  const auto &projected = result.wheels.front().slots.front();
  assert(projected.index == 2);
  assert(projected.archiveResource.value == "wheels/pattern.png");
  assert(projected.graphicWheelReference.value == "Graphic.Ref");
  assert(projected.graphicWheelResource.value == "graphics/pattern.png");
  assert(result.filters.front().name.value == "Blue");
}

} // namespace

// Exercises the GUI-independent GDTF Inspector detail projections.
int main() {
  CheckOverview();
  CheckMode();
  CheckWheels();
  return 0;
}
