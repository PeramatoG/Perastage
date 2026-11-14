/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "scene_alignment_bounds.h"
#include "geometry_bounds_resolver.h"
#include "filesystem_path_utils.h"
#include "../viewer3d/gdtfloader.h"
#include "matrixutils.h"
#include "support/archive_entry_test_utils.h"
#include "support/gdtf_test_fixture_builder.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <limits>
#include <wx/filename.h>

// This test supplies only a path lookup. All geometry uses production CPU loaders.
namespace viewer3d::resources {
std::string ResolveSceneResourcePath(const std::string &base,const std::string &ref) {
  auto path = PathUtils::PathFromUtf8(ref);
  return path.is_absolute() ? ref : (PathUtils::PathFromUtf8(base)/path).string();
}
}
namespace {
bool Near(double a,double b) { return std::abs(a-b) < 0.1; }
}
int main(int argc,char **argv) {
  assert(argc == 2);
  const auto temporary = wxFileName::CreateTempFileName("alignment_bounds_").ToStdString();
  std::filesystem::remove(temporary);
  const std::filesystem::path root = temporary;
  std::filesystem::create_directories(root);
  const auto gdtf = root/"physical.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithModelDimensionsMeters(1,2,3).WriteArchive(gdtf);
  MvrScene scene; scene.basePath = root.string();
  Fixture f; f.uuid="fixture"; f.gdtfSpec=gdtf.string(); f.gdtfMode="Default";
  f.transform=MatrixUtils::EulerToMatrix(90,0,0); f.transform.o={100,200,300};
  scene.fixtures[f.uuid]=f;
  std::string diagnostic;
  const auto measured = alignment::SceneBounds(scene,{MvrNodeType::Fixture,f.uuid},diagnostic);
  assert(measured && alignment::Valid(*measured));
  assert(Near(measured->max[0]-measured->min[0],2000));
  assert(Near(measured->max[1]-measured->min[1],1000));
  assert(Near(measured->max[2]-measured->min[2],3000));
  auto repeated=alignment::SceneBounds(scene,{MvrNodeType::Fixture,f.uuid},diagnostic);
  assert(repeated && repeated->min==measured->min && repeated->max==measured->max);
  // A real bundled mesh validates loader units, instance transforms and caching.
  const auto model=std::filesystem::path(argv[1])/"library/scene_objects/Kara.3ds";
  const auto local=GeometryBoundsResolver::Resolve(model,&diagnostic);
  assert(local);
  SceneObject object; object.uuid="object";
  object.transform=MatrixUtils::EulerToMatrix(20,30,40); object.transform.o={1300,2400,3500};
  Matrix instance=MatrixUtils::Identity(); instance.o={120,-230,340}; instance.u[0]=2;
  object.geometries.push_back({model.string(),instance});
  scene.sceneObjects[object.uuid]=object;
  const auto count=GeometryBoundsResolver::ParseCountForTesting();
  const auto objectBounds=alignment::SceneBounds(scene,{MvrNodeType::SceneObject,object.uuid},diagnostic);
  assert(objectBounds && alignment::Valid(*objectBounds));
  alignment::Bounds localBounds;
  for (int axis=0;axis<3;++axis) { localBounds.min[axis]=local->minMm[axis]; localBounds.max[axis]=local->maxMm[axis]; }
  const auto expected=alignment::TransformBounds(localBounds,MatrixUtils::Multiply(object.transform,instance));
  assert(expected.min==objectBounds->min && expected.max==objectBounds->max);
  assert(count==GeometryBoundsResolver::ParseCountForTesting());
  Truss truss; truss.uuid="truss"; truss.symbolFile=model.string(); truss.transform=object.transform;
  scene.trusses[truss.uuid]=truss;
  assert(alignment::SceneBounds(scene,{MvrNodeType::Truss,truss.uuid},diagnostic));
  GroupObject nested; nested.uuid="nested"; nested.children={{MvrNodeType::Fixture,f.uuid}};
  GroupObject group; group.uuid="group";
  group.children={{MvrNodeType::GroupObject,nested.uuid},{MvrNodeType::SceneObject,object.uuid}};
  nested.parentGroupUuid=group.uuid;
  scene.fixtures[f.uuid].parentGroupUuid=nested.uuid;
  scene.fixtures[f.uuid].localTransform=f.transform;
  scene.fixtures[f.uuid].hasLocalTransform=true;
  scene.sceneObjects[object.uuid].parentGroupUuid=group.uuid;
  scene.sceneObjects[object.uuid].localTransform=object.transform;
  scene.sceneObjects[object.uuid].hasLocalTransform=true;
  scene.groupObjects[nested.uuid]=nested; scene.groupObjects[group.uuid]=group;
  const auto groupBounds=alignment::SceneBounds(scene,{MvrNodeType::GroupObject,group.uuid},diagnostic);
  assert(groupBounds);
  for (int axis=0;axis<3;++axis) {
    assert(groupBounds->min[axis]==std::min(measured->min[axis],objectBounds->min[axis]));
    assert(groupBounds->max[axis]==std::max(measured->max[axis],objectBounds->max[axis]));
  }
  scene.groupObjects[group.uuid].children.push_back({MvrNodeType::GroupObject,group.uuid});
  assert(!alignment::SceneBounds(scene,{MvrNodeType::GroupObject,group.uuid},diagnostic));
  scene.fixtures[f.uuid].gdtfSpec="missing.gdtf";
  assert(!alignment::SceneBounds(scene,{MvrNodeType::Fixture,f.uuid},diagnostic));
  const auto fallback=root/"dimension_fallback.gdtf";
  const std::string xml="<GDTF DataVersion=\"1.2\"><FixtureType Name=\"Unknown\"><Models>"
    "<Model Name=\"Body\" Length=\"1\" Width=\"2\" Height=\"3\"/>"
    "</Models><Geometries><Geometry Name=\"Root\" Model=\"Body\"/></Geometries>"
    "</FixtureType></GDTF>";
  assert(tests::archive::WriteStoredZipWithRawNames(fallback,{{"description.xml",xml}},diagnostic));
  scene.fixtures[f.uuid].gdtfSpec=fallback.string();
  assert(!alignment::SceneBounds(scene,{MvrNodeType::Fixture,f.uuid},diagnostic));
  const auto incomplete=root/"incomplete_primitive.gdtf";
  const std::string incompleteXml="<GDTF DataVersion=\"1.2\"><FixtureType Name=\"Unknown\"><Models>"
    "<Model Name=\"Body\" PrimitiveType=\"Cube\" Length=\"1\" Width=\"0\" Height=\"3\"/>"
    "</Models><Geometries><Geometry Name=\"Root\" Model=\"Body\"/></Geometries>"
    "</FixtureType></GDTF>";
  assert(tests::archive::WriteStoredZipWithRawNames(incomplete,{{"description.xml",incompleteXml}},diagnostic));
  scene.fixtures[f.uuid].gdtfSpec=incomplete.string();
  assert(!alignment::SceneBounds(scene,{MvrNodeType::Fixture,f.uuid},diagnostic));
  scene.sceneObjects[object.uuid].transform.u[1]=std::numeric_limits<float>::quiet_NaN();
  assert(!alignment::SceneBounds(scene,{MvrNodeType::SceneObject,object.uuid},diagnostic));
  ShutdownGdtfCache();
  std::filesystem::remove_all(root);
}
