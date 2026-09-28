#include "inspection/gdtf_inspector_presentation.h"

#include <sstream>

namespace gui::inspection {
namespace {

// Wraps an authored string while preserving its availability state.
GdtfInspectorValue Value(const std::string &value) {
  return {value, !value.empty()};
}

// Formats an authored physical number only when its presence flag is set.
GdtfInspectorValue Number(bool present, float value, const char *unit) {
  if (!present)
    return {};
  std::ostringstream text;
  text << value << ' ' << unit;
  return {text.str(), true};
}

} // namespace

// Projects FixtureType metadata without inventing absent optional values.
GdtfOverviewPresentation BuildGdtfOverviewPresentation(
    const gdtf::GdtfDescriptionSnapshot &description) {
  GdtfOverviewPresentation result;
  result.rows = {
      {GdtfOverviewField::DataVersion, Value(description.dataVersion)},
      {GdtfOverviewField::FixtureTypeName, Value(description.fixtureTypeName)},
      {GdtfOverviewField::FixtureTypeId, Value(description.fixtureTypeId)},
      {GdtfOverviewField::Manufacturer, Value(description.manufacturer)},
      {GdtfOverviewField::ShortName, Value(description.shortName)},
      {GdtfOverviewField::LongName, Value(description.longName)},
      {GdtfOverviewField::Description, Value(description.description)},
      {GdtfOverviewField::Thumbnail, Value(description.thumbnail)},
      {GdtfOverviewField::CreationDate, Value(description.createDate)},
      {GdtfOverviewField::Revision, Value(description.revision)},
      {GdtfOverviewField::Weight,
       Number(description.weightKgPresent, description.weightKg, "kg")},
      {GdtfOverviewField::PowerConsumption,
       Number(description.powerConsumptionWPresent,
              description.powerConsumptionW, "W")},
      {GdtfOverviewField::ModelColor, Value(description.modelColorHex)},
      {GdtfOverviewField::TrussCrossSectionType,
       Value(description.trussCrossSectionType)},
      {GdtfOverviewField::TrussCrossSection,
       Value(description.trussCrossSection)},
  };
  for (const auto &revision : description.revisions) {
    result.revisions.push_back({Value(revision.text), Value(revision.date),
                                Value(revision.userId),
                                Value(revision.modifiedBy)});
  }
  return result;
}

// Projects a mode through the established hierarchical browser presenter.
GdtfModePresentation
BuildGdtfInspectorModePresentation(const gdtf::GdtfDmxModeNode &mode) {
  return {Value(mode.description), Value(mode.geometry),
          mode.calculatedFootprint, BuildGdtfModeBrowserPresentation(&mode)};
}

// Projects lightweight wheel, filter, and archive-reference facts.
GdtfWheelsPresentation
BuildGdtfWheelsPresentation(const gdtf::GdtfWheelCatalog &catalog) {
  GdtfWheelsPresentation result;
  for (const auto &wheel : catalog.wheels) {
    GdtfWheelPresentation projected{Value(wheel.name), Value(wheel.type), {}};
    for (const auto &slot : wheel.slots) {
      projected.slots.push_back(
          {slot.index, Value(slot.name), Value(slot.rawColor),
           Value(slot.rawFilter), Value(slot.mediaFileName),
           Value(slot.resolvedResourcePath),
           Value(slot.graphicWheelReference),
           Value(slot.graphicWheelResource)});
    }
    result.wheels.push_back(std::move(projected));
  }
  for (const auto &filter : catalog.filters)
    result.filters.push_back({Value(filter.name), Value(filter.rawColor)});
  return result;
}

} // namespace gui::inspection
