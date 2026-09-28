#pragma once

#include "gdtf/editor/gdtf_document.h"
#include "gdtf/gdtf_mode_channel_browser.h"
#include "gdtf/gdtf_wheel_catalog.h"
#include "gdtf/gdtf_mode_browser_presenter.h"

#include <string>
#include <vector>

namespace gui::inspection {

enum class GdtfOverviewField {
  DataVersion,
  FixtureTypeName,
  FixtureTypeId,
  Manufacturer,
  ShortName,
  LongName,
  Description,
  Thumbnail,
  CreationDate,
  Revision,
  Weight,
  PowerConsumption,
  ModelColor,
  TrussCrossSectionType,
  TrussCrossSection,
};

struct GdtfInspectorValue {
  std::string value;
  bool available = false;
};

struct GdtfOverviewRow {
  GdtfOverviewField field;
  GdtfInspectorValue value;
};

struct GdtfRevisionPresentation {
  GdtfInspectorValue text;
  GdtfInspectorValue date;
  GdtfInspectorValue userId;
  GdtfInspectorValue modifiedBy;
};

struct GdtfOverviewPresentation {
  std::vector<GdtfOverviewRow> rows;
  std::vector<GdtfRevisionPresentation> revisions;
};

struct GdtfModePresentation {
  GdtfInspectorValue description;
  GdtfInspectorValue geometry;
  int calculatedFootprint = 0;
  std::vector<GdtfModeBrowserNodePresentation> nodes;
};

struct GdtfWheelSlotPresentation {
  int index = 0;
  GdtfInspectorValue name;
  GdtfInspectorValue rawColor;
  GdtfInspectorValue filter;
  GdtfInspectorValue mediaReference;
  GdtfInspectorValue archiveResource;
  GdtfInspectorValue graphicWheelReference;
  GdtfInspectorValue graphicWheelResource;
};

struct GdtfWheelPresentation {
  GdtfInspectorValue name;
  GdtfInspectorValue type;
  std::vector<GdtfWheelSlotPresentation> slots;
};

struct GdtfFilterPresentation {
  GdtfInspectorValue name;
  GdtfInspectorValue rawColor;
};

struct GdtfWheelsPresentation {
  std::vector<GdtfWheelPresentation> wheels;
  std::vector<GdtfFilterPresentation> filters;
};

GdtfOverviewPresentation BuildGdtfOverviewPresentation(
    const gdtf::GdtfDescriptionSnapshot &description);
GdtfModePresentation
BuildGdtfInspectorModePresentation(const gdtf::GdtfDmxModeNode &mode);
GdtfWheelsPresentation
BuildGdtfWheelsPresentation(const gdtf::GdtfWheelCatalog &catalog);

} // namespace gui::inspection
