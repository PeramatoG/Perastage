#include "gdtf/gdtf_mode_channel_browser.h"

#include <cassert>
#include <string>

int main() {
  auto plain = gdtf::ParseGdtfDmxValue(" 255 ");
  assert(plain.valid && plain.value == 255 && plain.byteCount == 1);
  auto two = gdtf::ParseGdtfDmxValue("65535/2");
  assert(two.valid && two.normalized == 65535 && two.byteCount == 2);
  auto shifted = gdtf::ParseGdtfDmxValue("255/1s", 2);
  assert(shifted.valid && shifted.normalized == 65280);
  assert(!gdtf::ParseGdtfDmxValue("999/1").valid);

  const std::string xml = R"(<GDTF><FixtureType Name="Unicode">
    <AttributeDefinitions><Attributes>
      <Attribute Name="Dimmer" Pretty="Dimmer pretty" PhysicalUnit="Percent" Feature="Dim" ActivationGroup="Intensity" MainAttribute="Dimmer" Color="0,1,1"/>
      <Attribute Name="Pan" Pretty="Pan pretty" PhysicalUnit="Degree" Feature="Position"/>
    </Attributes></AttributeDefinitions>
    <DMXModes>
      <DMXMode Name="Mode Ω" Description="Desc" Geometry="Base"><DMXChannels>
        <DMXChannel DMXBreak="1" Offset="1,2" InitialFunction="Dim" Highlight="255/1" Geometry="Head">
          <LogicalChannel Attribute="Dimmer" Snap="No" Master="Grand" MibFade="1" DMXChangeTimeLimit="2">
            <ChannelFunction Name="Low ✓" Attribute="Dimmer" DMXFrom="0/2" PhysicalFrom="0" PhysicalTo="50" ModeMaster="Pan" CustomName="Custom">
              <ChannelSet Name="Closed" DMXFrom="0/2" WheelSlotIndex="1"><SubChannelSet Name="Sub" PhysicalFrom="0" PhysicalTo="1" SubPhysicalUnit="Percent" DMXProfile="Prof"/></ChannelSet>
              <ChannelSet Name="Open" DMXFrom="10/2"/>
            </ChannelFunction>
            <ChannelFunction Name="High" Attribute="Dimmer" DMXFrom="100/2" PhysicalFrom="50" PhysicalTo="100"/>
          </LogicalChannel>
        </DMXChannel>
        <DMXChannel Offset="None"><LogicalChannel Attribute="Missing"><ChannelFunction Name="Virtual" DMXFrom="0/1"/></LogicalChannel></DMXChannel>
      </DMXChannels></DMXMode>
      <DMXMode Name="Second"/>
    </DMXModes>
  </FixtureType></GDTF>)";
  auto doc = gdtf::ReadGdtfModeChannelDocument(xml);
  assert(doc.modes.size() == 2);
  const auto *mode = doc.FindMode("Mode Ω");
  assert(mode && mode->channels.size() == 2);
  assert(mode->channels[0].resolution == 2);
  assert(mode->channels[1].virtualChannel);
  const auto &logical = mode->channels[0].logicalChannels[0];
  assert(logical.attributeInfo.physicalUnit == "Percent");
  const auto &fn = logical.channelFunctions[0];
  assert(fn.effectiveDmxRange && fn.effectiveDmxRange->start == 0 && fn.effectiveDmxRange->end == 99);
  assert(fn.channelSets[0].effectiveDmxRange && fn.channelSets[0].effectiveDmxRange->end == 9);
  assert(fn.channelSets[1].effectivePhysicalRange.from == "0");
  assert(fn.channelSets[0].subChannelSets[0].id.find("subset[0]") != std::string::npos);
  assert(!doc.diagnostics.empty());
  auto bad = gdtf::ReadGdtfModeChannelDocument("<GDTF><FixtureType>");
  assert(!bad.diagnostics.empty());
  return 0;
}
