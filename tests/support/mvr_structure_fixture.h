#pragma once
#include <string>
#include <cstdint>
namespace tests::mvr_structure {
inline constexpr auto layer = "11000000-0000-4000-8000-000000000001";
inline constexpr auto focus = "61000000-0000-4000-8000-000000000001";
inline constexpr auto video = "62000000-0000-4000-8000-000000000001";
inline constexpr auto projector = "63000000-0000-4000-8000-000000000001";
inline constexpr auto child = "52000000-0000-4000-8000-000000000001";
inline constexpr auto truss = "64000000-0000-4000-8000-000000000001";
inline constexpr auto support = "65000000-0000-4000-8000-000000000001";
inline constexpr auto classId = "66000000-0000-4000-8000-000000000001";
inline constexpr auto definition = "a0000000-0000-4000-8000-000000000001";
inline constexpr auto nested = "a0000000-0000-4000-8000-000000000002";
inline constexpr auto innerSymbol = "b0000000-0000-4000-8000-000000000001";
inline constexpr auto focusSymbol = "b0000000-0000-4000-8000-000000000002";
inline constexpr auto trussSymbol = "b0000000-0000-4000-8000-000000000003";
inline constexpr auto supportSymbol = "b0000000-0000-4000-8000-000000000004";
inline constexpr auto secondFocusSymbol =
    "b0000000-0000-4000-8000-000000000005";
inline std::string Matrix(int x, int y = 0, int z = 0) {
  return "<Matrix>{1,0,0}{0,1,0}{0,0,1}{" + std::to_string(x) + "," +
         std::to_string(y) + "," + std::to_string(z) + "}</Matrix>";
}
inline std::string Symbol(const std::string &id, const std::string &target,
                          int x) {
  return "<Symbol uuid='" + id + "' symdef='" + target + "'>" + Matrix(x) +
         "</Symbol>";
}
inline std::string Chunk(std::uint16_t type, const std::string &payload) {
  std::string out;
  out.push_back(char(type));
  out.push_back(char(type >> 8));
  const auto size = std::uint32_t(payload.size() + 6);
  for (int byte = 0; byte < 4; ++byte)
    out.push_back(char(size >> (byte * 8)));
  return out + payload;
}
inline std::string Mesh(bool dependencies) {
  if (!dependencies)
    return Chunk(0x4d4d, {});
  std::string texture = "textures/mask.png";
  texture.push_back('\0');
  return Chunk(
      0x4d4d,
      Chunk(0x3d3d, Chunk(0xafff, Chunk(0xa210, Chunk(0xa300, texture)))));
}
inline std::string Texture() {
  const unsigned char png[] = {
      137, 80,  78,  71,  13, 10, 26, 10, 0,   0,  0,  13, 73,  72,
      68,  82,  0,   0,   0,  1,  0,  0,  0,   1,  8,  4,  0,   0,
      0,   181, 28,  12,  2,  0,  0,  0,  11,  73, 68, 65, 84,  120,
      218, 99,  100, 248, 15, 0,  1,  5,  1,   1,  39, 24, 227, 102,
      0,   0,   0,   0,   73, 69, 78, 68, 174, 66, 96, 130};
  return {reinterpret_cast<const char *>(png), sizeof(png)};
}
inline std::string Xml(const std::string &fixture) {
  const auto geometry = "<Geometries><Geometry3D fileName='mesh.3ds'>" +
                        Matrix(2) + "</Geometry3D></Geometries>";
  return "<GeneralSceneDescription verMajor='1' verMinor='6' provider='Test' "
         "providerVersion='1'><Scene><AUXData><Class uuid='" +
         std::string(classId) +
         "' name='Media'/>"
         "<Symdef uuid='" +
         std::string(definition) +
         "' name='Geometry'><ChildList><Geometry3D fileName='mesh.3ds'>" +
         Matrix(3) +
         "</Geometry3D></ChildList></Symdef>"
         "<Symdef uuid='" +
         nested + "' name='Nested'><ChildList>" +
         Symbol(innerSymbol, definition, 4) +
         "</ChildList></Symdef>"
         "</AUXData><Layers><Layer uuid='" +
         layer + "' name='Stage'>" + Matrix(0, 0, 25) +
         "<ChildList>"
         "<Fixture uuid='" +
         fixture + "' name='Original'>" + Matrix(100, 200, 300) +
         "<GDTFSpec>fixture.gdtf</GDTFSpec><GDTFMode>Mode</GDTFMode><Focus>" +
         focus +
         "</Focus><FixtureID>1</FixtureID><UnitNumber>1</UnitNumber>"
         "<CastShadow>true</CastShadow><CustomCommands><CustomCommand>Pan,f "
         "50</CustomCommand></CustomCommands><ChildList>"
         "<FocusPoint uuid='" +
         focus + "' name='Target'>" + Matrix(7) +
         "<Geometries>"
         "<Geometry3D fileName='mesh.3ds'>" +
         Matrix(31) + "</Geometry3D>" + Symbol(focusSymbol, nested, 5) +
         Symbol(secondFocusSymbol, definition, 29) +
         "</Geometries></FocusPoint>"
         "</ChildList></Fixture>"
         "<VideoScreen uuid='" +
         video + "' name='Screen'>" + Matrix(11) + "<Classing>" + classId +
         "</Classing>" + geometry +
         "<Sources><Source linkedGeometry='screen' "
         "type='File'>movie.bin</Source></Sources><FixtureID>2</"
         "FixtureID><FixtureIDNumeric>4294967296</"
         "FixtureIDNumeric></VideoScreen>"
         "<Projector uuid='" +
         projector + "' name='Projector'>" + Matrix(13) + geometry +
         "<Projections><Projection><Source linkedGeometry='lens' "
         "type='File'>movie.bin</Source></Projection></Projections><ChildList>"
         "<Fixture uuid='" +
         child + "' name='Child' multipatch='" + fixture + "'>" + Matrix(17) +
         "<GDTFSpec>fixture.gdtf</GDTFSpec><GDTFMode>Mode</"
         "GDTFMode><FixtureID>3</FixtureID><UnitNumber>2</UnitNumber></Fixture>"
         "</ChildList><FixtureID>4</FixtureID></Projector>"
         "<Truss uuid='" +
         truss + "' name='Rig'>" + Matrix(19) + "<Geometries>" +
         Symbol(trussSymbol, nested, 6) +
         "</Geometries><FixtureID>5</FixtureID></Truss>"
         "<Support uuid='" +
         support + "' name='Support'>" + Matrix(23) + "<Geometries>" +
         Symbol(supportSymbol, definition, 8) +
         "</Geometries><ChainLength>1</ChainLength><FixtureID>6</FixtureID></"
         "Support>"
         "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
}
} // namespace tests::mvr_structure
