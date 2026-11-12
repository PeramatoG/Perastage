#include "gdtf/gdtf_mode_browser_presenter.h"

#include <cassert>
#include <string>

// Verifies top-level mode browser labels include immediate channel contents.
int main() {
  const std::string xml = R"(<GDTF><FixtureType Name="Presenter">
    <AttributeDefinitions><Attributes>
      <Attribute Name="Pan" Pretty="Pan"/>
      <Attribute Name="Shutter1" Pretty="Shutter"/>
    </Attributes></AttributeDefinitions>
    <DMXModes><DMXMode Name="Mode"><DMXChannels>
      <DMXChannel Offset="1"><LogicalChannel Attribute="Shutter1"><ChannelFunction Attribute="Shutter1" DMXFrom="0/1"/></LogicalChannel></DMXChannel>
      <DMXChannel Offset="2,3"><LogicalChannel Attribute="Pan"><ChannelFunction Attribute="Pan" DMXFrom="0/2"/></LogicalChannel></DMXChannel>
    </DMXChannels></DMXMode></DMXModes>
  </FixtureType></GDTF>)";

  const auto doc = gdtf::ReadGdtfModeChannelDocument(xml);
  const auto *mode = doc.FindMode("Mode");
  const auto rows = BuildGdtfModeBrowserPresentation(mode);

  assert(rows.size() >= 4);
  assert(rows[0].item == "Ch 1 - Shutter1");
  assert(rows[3].item == "Ch 2, 3 - Pan, Pan Fine");
  const auto unresolved = gdtf::ReadGdtfModeChannelDocument(R"(<GDTF><FixtureType>
    <Geometries><Geometry Name="Body"/></Geometries>
    <DMXModes><DMXMode Name="Unresolved" Geometry="Body"><DMXChannels>
      <DMXChannel Geometry="Body" DMXBreak="Overwrite" Offset="6"><LogicalChannel Attribute="Shutter1"/></DMXChannel>
    </DMXChannels></DMXMode></DMXModes></FixtureType></GDTF>)");
  const auto *unresolvedMode = unresolved.FindMode("Unresolved");
  assert(!unresolvedMode->channels[0].effectiveDmxBreak);
  assert(!unresolvedMode->channels[0].offsets[0].normalized);
  assert(BuildGdtfModeBrowserPresentation(unresolvedMode)[0].item == "Ch 6 - Shutter1");
  assert(BuildGdtfModeChannelSummaryPresentation(unresolvedMode)[0].channelLabel == "6");
  return 0;
}
