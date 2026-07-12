#include "gdtf/gdtf_color_cie.h"
#include "gdtf/gdtf_dmx_inspector.h"
#include "gdtf/gdtf_mode_channel_browser.h"
#include "gdtf/gdtf_wheel_catalog.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {
const char *kXml = R"xml(<GDTF><FixtureType>
  <AttributeDefinitions><Attributes><Attribute Name="Gobo1" PhysicalUnit="None"/></Attributes></AttributeDefinitions>
  <Filters><Filter Name="Fílter Blue" Color="0.15,0.06,0.4"/></Filters>
  <Wheels>
    <Wheel Name="GoboWheel" Type="Gobo"><Slot Name="Open" Color="0.3127,0.3290,1"/><Slot Name="Góbø" MediaFileName="wheels/gobo.png" Filter="Fílter Blue"><PrismFacet Name="FacetA"/></Slot></Wheel>
    <Wheel Name="GraphicWheel" Type="Graphic"><WheelSlot Name="GraphicSlot" MediaFileName="graphics/pattern.png" GraphicWheelResource="graphics/pattern.png"><AnimationSystem Name="AnimA"/></WheelSlot></Wheel>
  </Wheels>
  <DMXModes><DMXMode Name="Mode" Geometry="Body"><DMXChannels>
    <DMXChannel Offset="1"><LogicalChannel Attribute="Gobo1"><ChannelFunction Name="Gobo" Attribute="Gobo1" DMXFrom="0/1" PhysicalFrom="0" PhysicalTo="100" Wheel="GoboWheel"><ChannelSet Name="Open" DMXFrom="0/1" WheelSlotIndex="1"/><ChannelSet Name="Pattern" DMXFrom="128/1" WheelSlotIndex="2"/></ChannelFunction></LogicalChannel></DMXChannel>
    <DMXChannel Offset="2,3"><LogicalChannel Attribute="Gobo1"><ChannelFunction Name="Graphic" Attribute="Gobo1" DMXFrom="0/2" Wheel="GraphicWheel" ModeMaster="Control" DMXProfile="ProfileA"><ChannelSet Name="Graphic" DMXFrom="0/2" WheelSlotIndex="1"/></ChannelFunction></LogicalChannel></DMXChannel>
  </DMXChannels></DMXMode></DMXModes>
</FixtureType></GDTF>)xml";
}

int main() {
  const auto catalog = gdtf::ReadGdtfWheelCatalog(kXml);
  assert(catalog.wheels.size() == 2);
  assert(catalog.filters.size() == 1);
  assert(catalog.wheels[0].slots.size() == 2);
  assert(catalog.wheels[0].slots[1].index == 2);
  assert(catalog.wheels[0].slots[1].mediaFileName == "wheels/gobo.png");
  assert(catalog.wheels[0].slots[1].rawFilter == "Fílter Blue");
  assert(catalog.wheels[1].graphicWheel);
  assert(catalog.wheels[1].slots[0].graphicWheelResource == "graphics/pattern.png");
  assert(catalog.wheels[0].slots[0].color.valid);

  const auto srgb = gdtf::ConvertCieXyyToSrgb(catalog.wheels[0].slots[0].color);
  assert(srgb.valid);
  const auto malformed = gdtf::ParseGdtfColorCie("bad", gdtf::GdtfValueOrigin::Explicit);
  assert(!malformed.valid);

  const auto doc = gdtf::ReadGdtfModeChannelDocument(kXml);
  const auto *mode = doc.FindMode("Mode");
  assert(mode);
  auto first = gdtf::InspectGdtfDmxValue(*mode, mode->channels[0].id, 128, catalog);
  assert(first.bytes.size() == 1 && first.bytes[0] == 128);
  assert(first.mappings.size() == 1);
  assert(first.mappings[0].slot && first.mappings[0].slot->index == 2);
  assert(first.mappings[0].filter && first.mappings[0].filter->name == "Fílter Blue");
  assert(first.mappings[0].mediaResource == "wheels/gobo.png");
  auto second = gdtf::InspectGdtfDmxValue(*mode, mode->channels[1].id, 0x1234, catalog);
  assert(second.bytes.size() == 2 && second.bytes[0] == 0x12 && second.bytes[1] == 0x34);
  assert(second.mappings[0].modeMasterConditional);
  assert(second.mappings[0].physicalApproximate);
  assert(second.mappings[0].wheel && second.mappings[0].wheel->graphicWheel);
  assert(second.mappings[0].slot && second.mappings[0].graphicWheelResource == "graphics/pattern.png");
  std::cout << "GDTF wheel attribute inspector core checks passed\n";
}
