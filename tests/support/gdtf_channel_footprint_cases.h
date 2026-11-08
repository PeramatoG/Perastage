#pragma once
#include <string>
#include <map>
#include <vector>

namespace tests::gdtf {
inline std::string PixelDescription(int count, bool child) {
  std::string xml = "<GDTF DataVersion=\"1.2\"><FixtureType><Geometries><Geometry Name=\"Body\">";
  for (int i = 0; i < count; ++i)
    xml += "<GeometryReference Name=\"Pixel_" + std::to_string(i) +
           "\" Geometry=\"PixelUnit\"><Break DMXBreak=\"1\" DMXOffset=\"" +
           std::to_string(1 + 3 * i) + "\"/></GeometryReference>";
  xml += "</Geometry><Geometry Name=\"PixelUnit\"><Beam Name=\"PixelBeam\" BeamType=\"Glow\"/>"
         "</Geometry></Geometries><DMXModes><DMXMode Name=\"Pixels\" Geometry=\"Body\"><DMXChannels>";
  for (int i = 1; i <= 3; ++i)
    xml += "<DMXChannel Geometry=\"" + std::string(child ? "PixelBeam" : "PixelUnit") +
           "\" DMXBreak=\"Overwrite\" Offset=\"" + std::to_string(i) + "\"/>";
  return xml + "</DMXChannels></DMXMode></DMXModes></FixtureType></GDTF>";
}
}

namespace tests::gdtf {
struct FootprintCase {
  std::string xml;
  std::map<int, int> breaks;
  std::map<int, std::vector<int>> bytes;
  bool diagnostic = false;
};
inline std::string Description(const std::string& geometries, const std::string& channels,
                               const std::string& root = "Body") {
  return "<GDTF DataVersion=\"1.2\"><FixtureType><Geometries>" + geometries +
      "</Geometries><DMXModes><DMXMode Name=\"Pixels\" Geometry=\"" + root +
      "\"><DMXChannels>" + channels + "</DMXChannels></DMXMode></DMXModes></FixtureType></GDTF>";
}
inline std::vector<FootprintCase> FootprintCases() {
  const std::string body = "<Geometry Name=\"Body\"/>";
  auto channel = [](const std::string& offsets, const std::string& geometry = "Body",
                    const std::string& br = "1") {
    return "<DMXChannel Geometry=\"" + geometry + "\" DMXBreak=\"" + br +
        "\" Offset=\"" + offsets + "\"/>";
  };
  auto ref = [](const std::string& target, const std::string& breaks) {
    return "<Geometry Name=\"Body\"><GeometryReference Name=\"Instance\" Geometry=\"" +
        target + "\">" + breaks + "</GeometryReference></Geometry>";
  };
  std::vector<FootprintCase> result;
  result.push_back({Description(body, channel("1") + channel("2") + channel("3")),
                    {{1, 3}}, {{1, {1, 2, 3}}}});
  for (const auto count : {2, 60}) {
    for (bool child : {false, true}) {
      std::vector<int> bytes;
      for (int i = 1; i <= count * 3; ++i) bytes.push_back(i);
      result.push_back({PixelDescription(count, child), {{1, count * 3}}, {{1, bytes}}});
    }
  }
  result.push_back({Description(body, channel("1") + channel("5") + channel("10")),
                    {{1, 10}}, {{1, {1, 5, 10}}}});
  result.push_back({Description(body, channel("1,2") + channel(" None ") + "<DMXChannel/>"),
                    {{1, 2}}, {{1, {1, 2}}}});
  result.push_back({Description(body, channel("1,5,10,300")),
                    {{1, 300}}, {{1, {1, 5, 10, 300}}}});
  result.push_back({Description(body, channel("1,2") + channel("10", "Body", "2")),
                    {{1, 2}, {2, 10}}, {{1, {1, 2}}, {2, {10}}}, true});
  result.push_back({Description(
      "<Geometry Name=\"Body\"><GeometryReference Name=\"Set1\" Geometry=\"Pixel\"><Break DMXBreak=\"1\" DMXOffset=\"5\"/></GeometryReference>"
      "<GeometryReference Name=\"Set2\" Geometry=\"Pixel\"><Break DMXBreak=\"1\" DMXOffset=\"15\"/></GeometryReference></Geometry><Beam Name=\"Pixel\"/>",
      channel("1", "Pixel") + channel("2", "Pixel")), {{1, 16}}, {{1, {5, 6, 15, 16}}}});
  const auto pixel = "<Geometry Name=\"PixelUnit\"><Beam Name=\"PixelBeam\"/></Geometry>";
  result.push_back({Description(ref("PixelUnit", "<Break DMXBreak=\"3\" DMXOffset=\"5\"/>") + pixel,
                               channel("1,2", "PixelBeam", "Overwrite")),
                    {{3, 6}}, {{3, {5, 6}}}, true});
  result.push_back({Description(ref("PixelUnit", "<Break DMXBreak=\"2\" DMXOffset=\"5\"/>"
                                                "<Break DMXBreak=\"4\" DMXOffset=\"10\"/>") + pixel,
                               channel("1", "PixelBeam", "4") + channel("2", "PixelBeam", "2")),
                    {{2, 6}, {4, 10}}, {{2, {6}}, {4, {10}}}, true});
  result.push_back({Description(ref("PixelUnit", "<Break DMXBreak=\"1\" DMXOffset=\"5\"/>"
                                                "<Break DMXBreak=\"3\" DMXOffset=\"10\"/>") + pixel,
                               channel("1", "PixelBeam", "2") +
                               channel("2", "PixelBeam", "Overwrite") + channel("None", "PixelBeam")),
                    {{1, 5}, {3, 11}}, {{1, {5}}, {3, {11}}}, true});
  result.push_back({Description(ref("PixelUnit", "<Break/>") +
      "<Geometry Name=\"PixelUnit\"><GeometryReference Name=\"Nested\" Geometry=\"Other\"><Break/></GeometryReference></Geometry>"
      "<Geometry Name=\"Other\"><Beam Name=\"PixelBeam\"/></Geometry>",
      channel("1", "PixelBeam", "Overwrite")), {}, {}, true});
  result.push_back({Description(ref("PixelUnit", "<Break/>") +
      "<Geometry Name=\"PixelUnit\"><GeometryReference Name=\"Cycle\" Geometry=\"Body\"><Break/></GeometryReference></Geometry>",
      channel("1", "PixelBeam", "Overwrite")), {}, {}, true});
  result.push_back({Description(ref("PixelUnit", "<Break/>") +
      "<Geometry Name=\"PixelUnit\"><Beam Name=\"PixelBeam\"/><Beam Name=\"PixelBeam\"/></Geometry>",
      channel("1", "PixelBeam", "Overwrite")), {}, {}, true});
  result.push_back({Description(body + "<Geometry Name=\"Unused\"><GeometryReference Name=\"Bad\" Geometry=\"Missing\"/></Geometry>",
                               channel("1")), {{1, 1}}, {{1, {1}}}});
  for (const auto& geometry : {
       ref("Missing", "<Break/>") + pixel,
       ref("PixelBeam", "<Break/>") + pixel,
       ref("PixelUnit", "<Break/>") + pixel + pixel,
       ref("Body", "<Break/>") + pixel,
       ref("PixelUnit", "<Break DMXOffset=\"invalid\"/>") + pixel,
       ref("PixelUnit", "<Break DMXOffset=\"0\"/>") + pixel,
       ref("PixelUnit", "") + pixel,
       ref("PixelUnit", "<Break/><Break/>") + pixel}) {
    result.push_back({Description(geometry, channel("1", "PixelBeam", "Overwrite")), {}, {}, true});
  }
  result.push_back({Description(body, channel("1", "Missing")), {}, {}, true});
  result.push_back({Description(body, channel("1"), "Missing"), {}, {}, true});
  result.push_back({Description(body, channel("1", "Body", "Overwrite")), {}, {}, true});
  for (const auto* invalid : {"0", "-1", "1,bad", "2147483648", "1,2,3,4,5", "1,"})
    result.push_back({Description(body, channel(invalid)), {}, {}, true});
  return result;
}
}
