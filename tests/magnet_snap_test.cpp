#include "magnet_snap.h"

#include "matrixutils.h"
#include "scene_grouping.h"
#include "truss_attachment_candidates.h"
#include "wx_path_utils.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {

std::string_view g_stage = "startup";

// Builds a translated identity matrix.
Matrix Translated(float x, float y, float z) {
  Matrix matrix = MatrixUtils::Identity();
  matrix.o = {x, y, z};
  return matrix;
}

// Adds a truss with common test dimensions.
void AddTruss(MvrScene &scene, const std::string &uuid, float x) {
  Truss truss;
  truss.uuid = uuid;
  truss.layer = "No Layer";
  truss.lengthMm = 3000.0f;
  truss.widthMm = 300.0f;
  truss.heightMm = 300.0f;
  truss.position = "LX1";
  truss.positionName = "LX1";
  truss.transform = Translated(x, 0.0f, 0.0f);
  scene.trusses[uuid] = truss;
}

// Writes a minimal GDTF ZIP archive for production resolver tests.
bool WriteGdtf(const std::filesystem::path &path, const std::string &xml) {
  bool success = false;
  {
    wxFFileOutputStream file(WxPathUtils::WxStringFromFilesystemPath(path));
    if (!file.IsOk())
      return false;
    wxZipOutputStream zip(file);
    if (!zip.PutNextEntry("description.xml"))
      return false;
    zip.Write(xml.data(), xml.size());
    if (!zip.IsOk() || zip.LastWrite() != xml.size())
      return false;
    if (!zip.CloseEntry() || !zip.Close())
      return false;
    success = file.IsOk();
  }
  return success;
}

// Returns a minimal GDTF description with an optional Magnet.
std::string ArchiveXml(bool withMagnet, float xMeters = 0.0f,
                       const std::string &padding = {}) {
  const std::string magnet =
      withMagnet
          ? "<Magnet Name='ArchiveMagnet' Position='{1,0,0," +
                std::to_string(xMeters) + "}{0,1,0,0}{0,0,1,0}{0,0,0,1}'/>"
          : "";
  return "<GDTF><FixtureType><Geometries><Geometry Name='Root'>" + magnet +
         padding + "</Geometry></Geometries></FixtureType></GDTF>";
}

// Reports a non-modal test failure with its source location.
bool Check(bool condition, const char *expression, int line) {
  if (condition)
    return true;
  std::cerr << "CHECK failed in stage '" << g_stage << "' at line " << line
            << ": " << expression << std::endl;
  return false;
}

// Records and flushes concise context for any subsequent test failure or hang.
void SetStage(std::string_view stage) {
  g_stage = stage;
  std::cerr << "MagnetSnapCore stage: " << stage << std::endl;
}

#define CHECK_OR_RETURN(expression)                                            \
  do {                                                                         \
    if (!Check(static_cast<bool>(expression), #expression, __LINE__))          \
      return 1;                                                                \
  } while (false)

} // namespace

// Verifies Magnet snap candidates, metadata preservation, and committed
// grouping.
int main() {
  wxInitializer initializer;
  CHECK_OR_RETURN(initializer.IsOk());
  using truss_attachment::CandidateKind;
  SetStage("inferred candidates");
  const auto longitudinal = truss_attachment::BuildInferredCandidates(
      {3000.0f, 400.0f, 400.0f}, Translated(100.0f, 200.0f, 300.0f), "long");
  CHECK_OR_RETURN(longitudinal.size() == 2);
  CHECK_OR_RETURN(longitudinal[0].kind ==
                  CandidateKind::InferredLongitudinalEnd);
  CHECK_OR_RETURN(longitudinal[0].localTransform.o[0] == 0.0f);
  CHECK_OR_RETURN(longitudinal[1].localTransform.o[0] == 3000.0f);
  CHECK_OR_RETURN(longitudinal[0].worldTransform.o[0] == 100.0f);
  CHECK_OR_RETURN(longitudinal[1].worldTransform.o[0] == 3100.0f);
  CHECK_OR_RETURN(longitudinal[0].worldDirection &&
                  (*longitudinal[0].worldDirection)[0] == -1.0f);
  CHECK_OR_RETURN(longitudinal[1].worldDirection &&
                  (*longitudinal[1].worldDirection)[0] == 1.0f);
  GeometryBounds offsetBounds{{200.0f, -200.0f, -21.65f},
                              {3200.0f, 200.0f, 371.65f}};
  const auto offsetLongitudinal =
      truss_attachment::BuildInferredCandidatesFromBounds(
          offsetBounds, MatrixUtils::Identity(), "offset-long");
  CHECK_OR_RETURN(offsetLongitudinal.size() == 2);
  CHECK_OR_RETURN(offsetLongitudinal[0].localTransform.o[0] == 200.0f);
  CHECK_OR_RETURN(offsetLongitudinal[1].localTransform.o[0] == 3200.0f);
  CHECK_OR_RETURN(std::fabs(offsetLongitudinal[0].localTransform.o[1]) < 0.001f);
  CHECK_OR_RETURN(std::fabs(offsetLongitudinal[0].localTransform.o[2] - 175.0f) <
                  0.001f);
  CHECK_OR_RETURN(
      truss_attachment::ClassifyLongitudinalAxis({3000, 400, 400}) == 0);
  CHECK_OR_RETURN(
      truss_attachment::ClassifyLongitudinalAxis({400, 3000, 400}) == 1);
  CHECK_OR_RETURN(
      truss_attachment::ClassifyLongitudinalAxis({400, 400, 3000}) == 2);
  CHECK_OR_RETURN(!truss_attachment::ClassifyLongitudinalAxis({800, 400, 400}));
  CHECK_OR_RETURN(truss_attachment::ClassifyLongitudinalAxis({801, 400, 400}) ==
                  0);

  const auto ambiguous = truss_attachment::BuildInferredCandidates(
      {400.0f, 400.0f, 400.0f}, MatrixUtils::Identity(), "cube");
  CHECK_OR_RETURN(ambiguous.size() == 6);
  CHECK_OR_RETURN(ambiguous.front().stableId == "face-axis-0-negative");
  CHECK_OR_RETURN(ambiguous.back().stableId == "face-axis-2-positive");
  for (const auto &candidate : ambiguous)
    CHECK_OR_RETURN(candidate.kind == CandidateKind::InferredFaceCenter);
  CHECK_OR_RETURN(truss_attachment::BuildInferredCandidates(
                      {0.0f, 400.0f, 400.0f}, MatrixUtils::Identity())
                      .size() == 6);

  SetStage("visible anchor references");
  MvrScene anchorScene;
  AddTruss(anchorScene, "reference-truss", 100.0f);
  truss_attachment::CandidateResolver anchorResolver;
  const auto anchorReferences = magnet_snap::BuildAnchorReferences(
      anchorScene, {magnet_snap::ObjectType::Truss, "reference-truss"},
      anchorResolver);
  CHECK_OR_RETURN(anchorReferences.size() == 2);
  CHECK_OR_RETURN(anchorReferences.front().positionMm[0] == 100.0f);
  CHECK_OR_RETURN(anchorReferences.front().direction.has_value());

  SceneObject referenceObject;
  referenceObject.uuid = "reference-object";
  referenceObject.transform = Translated(500.0f, 0.0f, 0.0f);
  anchorScene.sceneObjects[referenceObject.uuid] = referenceObject;
  const auto objectReferences = magnet_snap::BuildAnchorReferences(
      anchorScene, {magnet_snap::ObjectType::SceneObject, referenceObject.uuid},
      anchorResolver);
  CHECK_OR_RETURN(objectReferences.size() == 12);

  SetStage("explicit XML Magnet parsing");
  const std::string magnetXml =
      "<GDTF><FixtureType><Geometries>"
      "<Geometry Name='Root' Position='{1,0,0,1}{0,1,0,2}{0,0,1,3}{0,0,0,1}'>"
      "<Magnet Name='A' Model='Connector' "
      "Position='{1,0,0,0.5}{0,1,0,0}{0,0,1,0}{0,0,0,1}'/>"
      "<Geometry><Magnet Name='B'/></Geometry>"
      "<Magnet Name='Bad' Position='invalid'/></Geometry>"
      "</Geometries></FixtureType></GDTF>";
  const auto explicitMagnets = truss_attachment::ReadExplicitGdtfMagnets(
      magnetXml, Translated(100.0f, 0.0f, 0.0f));
  CHECK_OR_RETURN(explicitMagnets.candidates.size() == 2);
  CHECK_OR_RETURN(explicitMagnets.diagnostics.size() == 1);
  CHECK_OR_RETURN(explicitMagnets.candidates[0].name == "A");
  CHECK_OR_RETURN(explicitMagnets.candidates[0].model == "Connector");
  CHECK_OR_RETURN(explicitMagnets.candidates[1].model.empty());
  CHECK_OR_RETURN(explicitMagnets.candidates[0].localTransform.o[0] == 1500.0f);
  CHECK_OR_RETURN(explicitMagnets.candidates[0].localTransform.o[1] == 2000.0f);
  CHECK_OR_RETURN(explicitMagnets.candidates[0].worldTransform.o[0] == 1600.0f);
  CHECK_OR_RETURN(!explicitMagnets.candidates[0].worldDirection);
  CHECK_OR_RETURN(truss_attachment::ReadExplicitGdtfMagnets(
                      "<GDTF><FixtureType><Geometries/></FixtureType></GDTF>",
                      MatrixUtils::Identity())
                      .candidates.empty());

  SetStage("temporary directory creation");
  const auto tempRoot =
      std::filesystem::temp_directory_path() /
      ("perastage-truss-attachment-test-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::error_code filesystemError;
  std::filesystem::create_directories(tempRoot / "resources", filesystemError);
  CHECK_OR_RETURN(!filesystemError);
  const auto publicArchive = tempRoot / "resources" / "public.gdtf";
  const auto auxiliaryArchive = tempRoot / "resources" / "auxiliary.gdtf";
  SetStage("initial GDTF archive writes");
  CHECK_OR_RETURN(WriteGdtf(publicArchive, ArchiveXml(true, 0.25f)));
  CHECK_OR_RETURN(WriteGdtf(auxiliaryArchive, ArchiveXml(true, 0.5f)));
  MvrScene resourceScene;
  resourceScene.basePath = tempRoot.string();
  Truss resourceTruss;
  resourceTruss.uuid = "resource-truss";
  resourceTruss.lengthMm = 3000;
  resourceTruss.widthMm = 400;
  resourceTruss.heightMm = 400;
  resourceTruss.gdtfSpec = "resources/public.gdtf";
  truss_attachment::CandidateResolver resolver;
  SetStage("relative resource resolution");
  auto resolved =
      truss_attachment::BuildCandidates(resourceScene, resourceTruss, resolver);
  CHECK_OR_RETURN(resolved.candidates.size() == 1);
  CHECK_OR_RETURN(resolved.candidates[0].kind ==
                  CandidateKind::ExplicitGdtfMagnet);
  CHECK_OR_RETURN(resolved.candidates[0].localTransform.o[0] == 250.0f);
  CHECK_OR_RETURN(resolver.ArchiveParseCount() == 1);
  resourceTruss.transform.o[1] = 2000.0f;
  resolved =
      truss_attachment::BuildCandidates(resourceScene, resourceTruss, resolver);
  CHECK_OR_RETURN(resolver.ArchiveParseCount() == 1);
  CHECK_OR_RETURN(resolved.candidates[0].worldTransform.o[1] == 2000.0f);

  resourceTruss.gdtfSpec = "resources/missing.gdtf";
  resourceTruss.perastageAuxGdtfArchivePath = "resources/auxiliary.gdtf";
  SetStage("auxiliary resource fallback");
  resolved =
      truss_attachment::BuildCandidates(resourceScene, resourceTruss, resolver);
  CHECK_OR_RETURN(resolved.candidates.size() == 1);
  CHECK_OR_RETURN(resolved.candidates[0].localTransform.o[0] == 500.0f);
  CHECK_OR_RETURN(!resolved.diagnostics.empty());
  CHECK_OR_RETURN(resolver.ArchiveParseCount() == 2);

  resourceTruss.gdtfSpec = publicArchive.string();
  resourceTruss.perastageAuxGdtfArchivePath.clear();
  resolved =
      truss_attachment::BuildCandidates(resourceScene, resourceTruss, resolver);
  CHECK_OR_RETURN(resolved.candidates.size() == 1);
  CHECK_OR_RETURN(resolver.ArchiveParseCount() == 2);
  const auto previousWriteTime =
      std::filesystem::last_write_time(publicArchive, filesystemError);
  CHECK_OR_RETURN(!filesystemError);
  const auto previousSize =
      std::filesystem::file_size(publicArchive, filesystemError);
  CHECK_OR_RETURN(!filesystemError);
  CHECK_OR_RETURN(WriteGdtf(publicArchive, ArchiveXml(true, 0.35f)));
  SetStage("same-metadata cache behavior");
  CHECK_OR_RETURN(std::filesystem::file_size(publicArchive, filesystemError) ==
                  previousSize);
  std::filesystem::last_write_time(publicArchive, previousWriteTime,
                                   filesystemError);
  CHECK_OR_RETURN(!filesystemError);
  resolved =
      truss_attachment::BuildCandidates(resourceScene, resourceTruss, resolver);
  CHECK_OR_RETURN(resolver.ArchiveParseCount() == 2);
  CHECK_OR_RETURN(resolved.candidates[0].localTransform.o[0] == 250.0f);
  SetStage("explicit cache Clear");
  resolver.Clear();
  resolved =
      truss_attachment::BuildCandidates(resourceScene, resourceTruss, resolver);
  CHECK_OR_RETURN(resolver.ArchiveParseCount() == 3);
  CHECK_OR_RETURN(resolved.candidates[0].localTransform.o[0] == 350.0f);

  SetStage("changed-version cache invalidation");
  CHECK_OR_RETURN(WriteGdtf(
      publicArchive, ArchiveXml(true, 0.75f, "<Geometry Name='Padding'/>")));
  std::filesystem::last_write_time(publicArchive,
                                   previousWriteTime + std::chrono::seconds(2),
                                   filesystemError);
  CHECK_OR_RETURN(!filesystemError);
  resolved =
      truss_attachment::BuildCandidates(resourceScene, resourceTruss, resolver);
  CHECK_OR_RETURN(resolver.ArchiveParseCount() == 4);
  CHECK_OR_RETURN(resolved.candidates[0].localTransform.o[0] == 750.0f);

  SetStage("malformed archive");
  const auto malformedArchive = tempRoot / "resources" / "malformed.gdtf";
  {
    std::ofstream malformedOutput(malformedArchive, std::ios::binary);
    CHECK_OR_RETURN(malformedOutput.is_open());
    malformedOutput << "not a zip";
    malformedOutput.close();
    CHECK_OR_RETURN(!malformedOutput.fail());
  }
  resourceTruss.gdtfSpec = malformedArchive.string();
  resolved =
      truss_attachment::BuildCandidates(resourceScene, resourceTruss, resolver);
  CHECK_OR_RETURN(resolved.candidates.size() == 2);
  CHECK_OR_RETURN(!resolved.diagnostics.empty());

  SetStage("no-Magnet archive");
  const auto noMagnetArchive = tempRoot / "resources" / "no-magnet.gdtf";
  CHECK_OR_RETURN(WriteGdtf(noMagnetArchive, ArchiveXml(false)));
  resourceTruss.gdtfSpec = noMagnetArchive.string();
  resolved =
      truss_attachment::BuildCandidates(resourceScene, resourceTruss, resolver);
  CHECK_OR_RETURN(resolved.candidates.size() == 2);

  SetStage("interactive resolver reuse");
  MvrScene cachedSnapScene;
  cachedSnapScene.basePath = tempRoot.string();
  AddTruss(cachedSnapScene, "cached-target", 0.0f);
  AddTruss(cachedSnapScene, "cached-source", 200.0f);
  cachedSnapScene.trusses["cached-target"].gdtfSpec =
      "resources/auxiliary.gdtf";
  cachedSnapScene.trusses["cached-source"].gdtfSpec =
      "resources/auxiliary.gdtf";
  truss_attachment::CandidateResolver interactiveResolver;
  magnet_snap::SnapSettings cachedSettings;
  cachedSettings.candidateResolver = &interactiveResolver;
  CHECK_OR_RETURN(magnet_snap::FindSnap(
      cachedSnapScene, {magnet_snap::ObjectType::Truss, "cached-source"},
      cachedSettings));
  CHECK_OR_RETURN(interactiveResolver.ArchiveParseCount() == 1);
  cachedSnapScene.trusses["cached-source"].transform.o[1] = 10.0f;
  CHECK_OR_RETURN(magnet_snap::FindSnap(
      cachedSnapScene, {magnet_snap::ObjectType::Truss, "cached-source"},
      cachedSettings));
  CHECK_OR_RETURN(interactiveResolver.ArchiveParseCount() == 1);

  SetStage("rotated candidates");
  Matrix rotated = MatrixUtils::EulerToMatrix(90.0f, 0.0f, 0.0f);
  const auto rotatedCandidates = truss_attachment::BuildInferredCandidates(
      {3000, 400, 400}, rotated, "rotated");
  CHECK_OR_RETURN(rotatedCandidates.size() == 2);
  CHECK_OR_RETURN(
      std::fabs(std::fabs(rotatedCandidates[1].worldTransform.o[1]) - 3000.0f) <
      0.001f);

  SetStage("straight group candidates");
  MvrScene straightGroupScene;
  GroupObject straightGroup;
  straightGroup.uuid = "straight-group";
  for (int index = 0; index < 3; ++index) {
    const std::string uuid = "straight-" + std::to_string(index);
    AddTruss(straightGroupScene, uuid, index * 3000.0f);
    straightGroupScene.trusses[uuid].parentGroupUuid = straightGroup.uuid;
    straightGroup.children.push_back({MvrNodeType::Truss, uuid});
  }
  straightGroupScene.groupObjects[straightGroup.uuid] = straightGroup;
  const auto straightCandidates = magnet_snap::BuildTrussGroupCandidates(
      straightGroupScene, straightGroup.uuid);
  CHECK_OR_RETURN(straightCandidates.size() == 2);
  CHECK_OR_RETURN(straightCandidates[0].worldTransform.o[0] == 0.0f);
  CHECK_OR_RETURN(straightCandidates[1].worldTransform.o[0] == 9000.0f);
  CHECK_OR_RETURN(straightCandidates[0].ownerTrussUuid == "straight-0");
  CHECK_OR_RETURN(straightCandidates[1].ownerTrussUuid == "straight-2");

  MvrScene rotatedGroupScene;
  GroupObject rotatedGroup;
  rotatedGroup.uuid = "rotated-group";
  for (int index = 0; index < 2; ++index) {
    const std::string uuid = "rotated-" + std::to_string(index);
    AddTruss(rotatedGroupScene, uuid, 0.0f);
    rotatedGroupScene.trusses[uuid].transform = rotated;
    rotatedGroupScene.trusses[uuid].transform.o[1] = -index * 3000.0f;
    rotatedGroupScene.trusses[uuid].parentGroupUuid = rotatedGroup.uuid;
    rotatedGroup.children.push_back({MvrNodeType::Truss, uuid});
  }
  rotatedGroupScene.groupObjects[rotatedGroup.uuid] = rotatedGroup;
  CHECK_OR_RETURN(magnet_snap::BuildTrussGroupCandidates(rotatedGroupScene,
                                                         rotatedGroup.uuid)
                      .size() == 2);

  SetStage("bent group member candidates");
  MvrScene bentGroupScene;
  GroupObject bentGroup;
  bentGroup.uuid = "bent-group";
  AddTruss(bentGroupScene, "bent-x", 0.0f);
  AddTruss(bentGroupScene, "bent-y", 3000.0f);
  bentGroupScene.trusses["bent-y"].transform = rotated;
  bentGroupScene.trusses["bent-y"].transform.o = {3000.0f, 0.0f, 0.0f};
  for (const std::string uuid : {"bent-x", "bent-y"}) {
    bentGroupScene.trusses[uuid].parentGroupUuid = bentGroup.uuid;
    bentGroup.children.push_back({MvrNodeType::Truss, uuid});
  }
  bentGroupScene.groupObjects[bentGroup.uuid] = bentGroup;
  const auto bentCandidates =
      magnet_snap::BuildTrussGroupCandidates(bentGroupScene, bentGroup.uuid);
  CHECK_OR_RETURN(bentCandidates.size() == 4);
  for (const auto &candidate : bentCandidates) {
    CHECK_OR_RETURN(candidate.kind == CandidateKind::InferredLongitudinalEnd);
    CHECK_OR_RETURN(candidate.ownerTrussUuid == "bent-x" ||
                    candidate.ownerTrussUuid == "bent-y");
  }
  AddTruss(bentGroupScene, "bent-moving", 250.0f);
  auto bentGroupSnap = magnet_snap::FindSnap(
      bentGroupScene, {magnet_snap::ObjectType::Truss, "bent-moving"});
  CHECK_OR_RETURN(bentGroupSnap);
  CHECK_OR_RETURN(bentGroupSnap->targetUuid == bentGroup.uuid);
  CHECK_OR_RETURN(bentGroupSnap->targetMemberTrussUuid == "bent-x");
  CHECK_OR_RETURN(bentGroupSnap->sourceCandidateId ==
                  "longitudinal-axis-0-positive");
  CHECK_OR_RETURN(std::fabs(bentGroupSnap->translationDeltaMm[0] + 3250.0f) <
                  0.001f);

  SetStage("explicit group member Magnet");
  MvrScene explicitGroupScene;
  explicitGroupScene.basePath = tempRoot.string();
  GroupObject explicitGroup;
  explicitGroup.uuid = "explicit-group";
  AddTruss(explicitGroupScene, "explicit-member", 0.0f);
  explicitGroupScene.trusses["explicit-member"].gdtfSpec =
      "resources/auxiliary.gdtf";
  explicitGroupScene.trusses["explicit-member"].parentGroupUuid =
      explicitGroup.uuid;
  explicitGroup.children.push_back({MvrNodeType::Truss, "explicit-member"});
  explicitGroupScene.groupObjects[explicitGroup.uuid] = explicitGroup;
  const auto explicitGroupCandidates = magnet_snap::BuildTrussGroupCandidates(
      explicitGroupScene, explicitGroup.uuid);
  CHECK_OR_RETURN(explicitGroupCandidates.size() == 1);
  CHECK_OR_RETURN(explicitGroupCandidates[0].kind ==
                  CandidateKind::ExplicitGdtfMagnet);
  CHECK_OR_RETURN(explicitGroupCandidates[0].ownerTrussUuid ==
                  "explicit-member");

  SetStage("basic truss snap");
  MvrScene scene;
  AddTruss(scene, "target", 0.0f);
  AddTruss(scene, "source", 3250.0f);

  auto snap =
      magnet_snap::FindSnap(scene, {magnet_snap::ObjectType::Truss, "source"});
  CHECK_OR_RETURN(snap);
  CHECK_OR_RETURN(snap->kind == magnet_snap::SnapKind::TrussToTruss);
  CHECK_OR_RETURN(snap->needsGrouping);
  CHECK_OR_RETURN(std::fabs(snap->translationDeltaMm[0] + 250.0f) < 0.001f);
  CHECK_OR_RETURN(snap->sourceCandidateId == "longitudinal-axis-0-negative");

  SetStage("left extension");
  MvrScene leftExtensionScene;
  AddTruss(leftExtensionScene, "target", 0.0f);
  AddTruss(leftExtensionScene, "source", 250.0f);
  const Matrix leftSourceBefore =
      leftExtensionScene.trusses["source"].transform;
  auto leftSnap = magnet_snap::FindSnap(
      leftExtensionScene, {magnet_snap::ObjectType::Truss, "source"});
  CHECK_OR_RETURN(leftSnap);
  CHECK_OR_RETURN(leftSnap->sourceCandidateId ==
                  "longitudinal-axis-0-positive");
  CHECK_OR_RETURN(std::fabs(leftSnap->translationDeltaMm[0] + 3250.0f) <
                  0.001f);
  CHECK_OR_RETURN(
      magnet_snap::ApplySnapTransform(leftExtensionScene, *leftSnap));
  CHECK_OR_RETURN(leftExtensionScene.trusses["source"].transform.u ==
                  leftSourceBefore.u);
  CHECK_OR_RETURN(leftExtensionScene.trusses["source"].transform.v ==
                  leftSourceBefore.v);
  CHECK_OR_RETURN(leftExtensionScene.trusses["source"].transform.w ==
                  leftSourceBefore.w);
  CHECK_OR_RETURN(leftExtensionScene.trusses["source"].transform.o[0] ==
                  -3000.0f);

  MvrScene ambiguousSourceScene;
  AddTruss(ambiguousSourceScene, "target", 0.0f);
  AddTruss(ambiguousSourceScene, "source", 240.0f);
  ambiguousSourceScene.trusses["source"].lengthMm = 400.0f;
  ambiguousSourceScene.trusses["source"].widthMm = 400.0f;
  ambiguousSourceScene.trusses["source"].heightMm = 400.0f;
  auto ambiguousSourceSnap = magnet_snap::FindSnap(
      ambiguousSourceScene, {magnet_snap::ObjectType::Truss, "source"});
  CHECK_OR_RETURN(ambiguousSourceSnap);
  CHECK_OR_RETURN(
      ambiguousSourceSnap->sourceCandidateId.rfind("face-axis-", 0) == 0);

  SetStage("rotated extension");
  MvrScene rotatedExtensionScene;
  AddTruss(rotatedExtensionScene, "target", 0.0f);
  AddTruss(rotatedExtensionScene, "source", 0.0f);
  rotatedExtensionScene.trusses["target"].transform = rotated;
  rotatedExtensionScene.trusses["source"].transform = rotated;
  for (int component = 0; component < 3; ++component)
    rotatedExtensionScene.trusses["source"].transform.o[component] =
        rotated.u[component] * 250.0f;
  auto rotatedLeftSnap = magnet_snap::FindSnap(
      rotatedExtensionScene, {magnet_snap::ObjectType::Truss, "source"});
  CHECK_OR_RETURN(rotatedLeftSnap);
  CHECK_OR_RETURN(rotatedLeftSnap->sourceCandidateId ==
                  "longitudinal-axis-0-positive");
  for (int component = 0; component < 3; ++component)
    CHECK_OR_RETURN(std::fabs(rotatedLeftSnap->translationDeltaMm[component] +
                              rotated.u[component] * 3250.0f) < 0.001f);

  SetStage("group extension");
  MvrScene groupExtensionScene;
  GroupObject targetGroup;
  targetGroup.uuid = "target-group";
  for (int index = 0; index < 2; ++index) {
    const std::string uuid = "target-member-" + std::to_string(index);
    AddTruss(groupExtensionScene, uuid, index * 3000.0f);
    groupExtensionScene.trusses[uuid].parentGroupUuid = targetGroup.uuid;
    targetGroup.children.push_back({MvrNodeType::Truss, uuid});
  }
  groupExtensionScene.groupObjects[targetGroup.uuid] = targetGroup;
  AddTruss(groupExtensionScene, "moving", 250.0f);
  auto groupLeftSnap = magnet_snap::FindSnap(
      groupExtensionScene, {magnet_snap::ObjectType::Truss, "moving"});
  CHECK_OR_RETURN(groupLeftSnap);
  CHECK_OR_RETURN(groupLeftSnap->targetUuid == targetGroup.uuid);
  CHECK_OR_RETURN(groupLeftSnap->targetMemberTrussUuid == "target-member-0");
  CHECK_OR_RETURN(groupLeftSnap->targetCandidateId ==
                  "longitudinal-axis-0-negative");
  CHECK_OR_RETURN(groupLeftSnap->sourceCandidateId ==
                  "longitudinal-axis-0-positive");
  CHECK_OR_RETURN(std::fabs(groupLeftSnap->translationDeltaMm[0] + 3250.0f) <
                  0.001f);

  scene.trusses["source"].transform.o[0] = 4000.0f;
  CHECK_OR_RETURN(!magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::Truss, "source"}));

  MvrScene sideSurfaceScene;
  AddTruss(sideSurfaceScene, "target", 0.0f);
  AddTruss(sideSurfaceScene, "source", 0.0f);
  sideSurfaceScene.trusses["source"].transform.o[1] = 550.0f;
  magnet_snap::SnapSettings topSideSettings;
  topSideSettings.axisWeights[2] = 0.0f;
  auto sideSurfaceSnap = magnet_snap::FindSnap(
      sideSurfaceScene, {magnet_snap::ObjectType::Truss, "source"},
      topSideSettings);
  CHECK_OR_RETURN(!sideSurfaceSnap);

  SetStage("Viewer3D screen-space cases");
  MvrScene screenScene;
  AddTruss(screenScene, "screen-target", 0.0f);
  AddTruss(screenScene, "screen-source", 0.0f);
  screenScene.trusses["screen-target"].transform.o[2] = 500.0f;
  magnet_snap::SnapSettings screenSettings;
  truss_attachment::CandidateResolver screenResolver;
  screenSettings.candidateResolver = &screenResolver;
  truss_screen_snap::ProjectionSnapshot screenProjection;
  screenProjection.modelView = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  screenProjection.projection = screenProjection.modelView;
  screenProjection.viewport = {0, 0, 1000, 1000};
  screenProjection.contentScale = 1.0;
  screenSettings.trussProjection = screenProjection;
  auto screenSnap = magnet_snap::FindSnap(
      screenScene, {magnet_snap::ObjectType::Truss, "screen-source"},
      screenSettings);
  CHECK_OR_RETURN(screenSnap);
  CHECK_OR_RETURN(std::fabs(screenSnap->translationDeltaMm[2] - 500.0f) <
                  0.001f);

  MvrScene screenLeftScene;
  AddTruss(screenLeftScene, "target", 0.0f);
  AddTruss(screenLeftScene, "source", 250.0f);
  auto screenLeftSettings = screenSettings;
  screenLeftSettings.trussProjection->viewport = {0, 0, 100, 100};
  auto screenLeftSnap = magnet_snap::FindSnap(
      screenLeftScene, {magnet_snap::ObjectType::Truss, "source"},
      screenLeftSettings);
  CHECK_OR_RETURN(screenLeftSnap);
  CHECK_OR_RETURN(screenLeftSnap->sourceCandidateId ==
                  "longitudinal-axis-0-positive");
  CHECK_OR_RETURN(std::fabs(screenLeftSnap->translationDeltaMm[0] + 3250.0f) <
                  0.001f);
  screenScene.trusses["screen-target"].transform.o[1] = 100.0f;
  CHECK_OR_RETURN(!magnet_snap::FindSnap(
      screenScene, {magnet_snap::ObjectType::Truss, "screen-source"},
      screenSettings));
  screenScene.trusses["screen-target"].transform.o[2] = 0.0f;
  screenScene.trusses["screen-target"].transform.o[1] = 40.0f;
  CHECK_OR_RETURN(!magnet_snap::FindSnap(
      screenScene, {magnet_snap::ObjectType::Truss, "screen-source"},
      screenSettings));

  MvrScene depthRankScene;
  AddTruss(depthRankScene, "source", 0.0f);
  AddTruss(depthRankScene, "far-depth", 0.0f);
  AddTruss(depthRankScene, "near-depth", 0.0f);
  depthRankScene.trusses["far-depth"].transform.o[2] = 600.0f;
  depthRankScene.trusses["near-depth"].transform.o[2] = 400.0f;
  auto rankedSnap = magnet_snap::FindSnap(
      depthRankScene, {magnet_snap::ObjectType::Truss, "source"},
      screenSettings);
  CHECK_OR_RETURN(rankedSnap && rankedSnap->targetUuid == "near-depth");

  auto flattenedProjection = screenProjection;
  flattenedProjection.projection[5] = 0.0;
  screenSettings.trussProjection = flattenedProjection;
  MvrScene worldRankScene;
  AddTruss(worldRankScene, "source", 0.0f);
  AddTruss(worldRankScene, "far-world", 0.0f);
  AddTruss(worldRankScene, "near-world", 0.0f);
  worldRankScene.trusses["far-world"].transform.o[1] = 200.0f;
  worldRankScene.trusses["near-world"].transform.o[1] = 100.0f;
  rankedSnap = magnet_snap::FindSnap(worldRankScene,
                                     {magnet_snap::ObjectType::Truss, "source"},
                                     screenSettings);
  CHECK_OR_RETURN(rankedSnap && rankedSnap->targetUuid == "near-world");

  MvrScene identityRankScene;
  AddTruss(identityRankScene, "source", 0.0f);
  AddTruss(identityRankScene, "z-target", 0.0f);
  AddTruss(identityRankScene, "a-target", 0.0f);
  rankedSnap = magnet_snap::FindSnap(identityRankScene,
                                     {magnet_snap::ObjectType::Truss, "source"},
                                     screenSettings);
  CHECK_OR_RETURN(rankedSnap && rankedSnap->targetUuid == "a-target");
  CHECK_OR_RETURN(!rankedSnap->sourceCandidateId.empty());
  CHECK_OR_RETURN(!rankedSnap->targetCandidateId.empty());
  screenSettings.trussProjection = screenProjection;

  SetStage("committed grouping");
  scene.trusses["source"].transform.o[0] = 3250.0f;
  snap =
      magnet_snap::FindSnap(scene, {magnet_snap::ObjectType::Truss, "source"});
  CHECK_OR_RETURN(snap);
  const std::string hang = scene.trusses["source"].positionName;
  CHECK_OR_RETURN(magnet_snap::ApplySnapTransform(scene, *snap));
  CHECK_OR_RETURN(scene.trusses["source"].positionName == hang);
  CHECK_OR_RETURN(magnet_snap::ApplyCommittedSnapGrouping(scene, *snap));
  CHECK_OR_RETURN(scene.groupObjects.size() == 1);
  const std::string groupUuid = scene.trusses["target"].parentGroupUuid;
  CHECK_OR_RETURN(!groupUuid.empty());

  AddTruss(scene, "loose", 6250.0f);
  auto groupSnap = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::TrussGroup, groupUuid});
  CHECK_OR_RETURN(groupSnap);
  CHECK_OR_RETURN(groupSnap->kind == magnet_snap::SnapKind::TrussToTruss);
  CHECK_OR_RETURN(groupSnap->sourceUuid == groupUuid);
  CHECK_OR_RETURN(groupSnap->sourceMemberTrussUuid == "source");
  CHECK_OR_RETURN(groupSnap->targetUuid == "loose");
  CHECK_OR_RETURN(std::fabs(groupSnap->translationDeltaMm[0] - 250.0f) <
                  0.001f);
  scene.trusses.erase("loose");

  AddTruss(scene, "source-2", 6250.0f);
  auto snap2 = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::Truss, "source-2"});
  CHECK_OR_RETURN(snap2);
  CHECK_OR_RETURN(snap2->targetUuid == groupUuid);
  CHECK_OR_RETURN(magnet_snap::ApplySnapTransform(scene, *snap2));
  CHECK_OR_RETURN(magnet_snap::ApplyCommittedSnapGrouping(scene, *snap2));
  CHECK_OR_RETURN(scene.trusses["source-2"].parentGroupUuid == groupUuid);
  CHECK_OR_RETURN(scene.groupObjects.size() == 1);

  AddTruss(scene, "interior-candidate", 3250.0f);
  CHECK_OR_RETURN(!magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::Truss, "interior-candidate"}));
  scene.trusses.erase("interior-candidate");

  SetStage("hidden-axis cases");
  magnet_snap::SnapSettings topViewSettings;
  topViewSettings.axisWeights[2] = 0.0f;
  AddTruss(scene, "top-view-source", 9250.0f);
  scene.trusses["top-view-source"].transform.o[2] = 1000.0f;
  auto topViewSnap = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::Truss, "top-view-source"},
      topViewSettings);
  CHECK_OR_RETURN(topViewSnap);
  CHECK_OR_RETURN(std::fabs(topViewSnap->translationDeltaMm[2]) > 0.001f);
  scene.trusses.erase("top-view-source");

  for (const auto &[hiddenAxis, sourceId] :
       std::vector<std::pair<int, std::string>>{{2, "bottom-view-source"},
                                                {1, "front-view-source"},
                                                {0, "side-view-source"}}) {
    magnet_snap::SnapSettings viewSettings;
    viewSettings.axisWeights[hiddenAxis] = 0.0f;
    AddTruss(scene, sourceId, hiddenAxis == 0 ? 20000.0f : 9250.0f);
    if (hiddenAxis != 0)
      scene.trusses[sourceId].transform.o[hiddenAxis] = 1000.0f;
    auto viewSnap = magnet_snap::FindSnap(
        scene, {magnet_snap::ObjectType::Truss, sourceId}, viewSettings);
    CHECK_OR_RETURN(viewSnap);
    CHECK_OR_RETURN(std::fabs(viewSnap->translationDeltaMm[hiddenAxis]) >
                    0.001f);
    scene.trusses.erase(sourceId);
  }

  SetStage("fixture regressions");
  MvrScene elevatedFixtureScene;
  AddTruss(elevatedFixtureScene, "elevated-truss", 0.0f);
  elevatedFixtureScene.trusses["elevated-truss"].transform.o[2] = 10000.0f;
  Fixture elevatedFixture;
  elevatedFixture.uuid = "elevated-fixture";
  elevatedFixture.transform = Translated(1510.0f, 160.0f, 3000.0f);
  elevatedFixtureScene.fixtures[elevatedFixture.uuid] = elevatedFixture;
  auto elevatedFixtureSnap = magnet_snap::FindSnap(
      elevatedFixtureScene,
      {magnet_snap::ObjectType::Fixture, elevatedFixture.uuid},
      topViewSettings);
  CHECK_OR_RETURN(elevatedFixtureSnap);
  CHECK_OR_RETURN(elevatedFixtureSnap->targetUuid == "elevated-truss");
  CHECK_OR_RETURN(
      std::fabs(elevatedFixtureSnap->translationDeltaMm[1] + 10.0f) < 0.001f);
  CHECK_OR_RETURN(
      std::fabs(elevatedFixtureSnap->translationDeltaMm[2] - 7000.0f) < 0.001f);
  CHECK_OR_RETURN(magnet_snap::ApplySnapTransform(elevatedFixtureScene,
                                                  *elevatedFixtureSnap));
  CHECK_OR_RETURN(
      std::fabs(
          elevatedFixtureScene.fixtures[elevatedFixture.uuid].transform.o[2] -
          10000.0f) < 0.001f);

  Fixture fixture;
  fixture.uuid = "fixture";
  fixture.layer = "MAC500";
  fixture.transform = Translated(1510.0f, 160.0f, 0.0f);
  scene.fixtures[fixture.uuid] = fixture;
  auto fixtureSnap = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::Fixture, fixture.uuid});
  CHECK_OR_RETURN(fixtureSnap);
  CHECK_OR_RETURN(fixtureSnap->kind == magnet_snap::SnapKind::FixtureToTruss);
  CHECK_OR_RETURN(fixtureSnap->needsGrouping);
  CHECK_OR_RETURN(std::fabs(fixtureSnap->translationDeltaMm[1] + 10.0f) <
                  0.001f);
  CHECK_OR_RETURN(std::fabs(fixtureSnap->translationDeltaMm[2]) < 0.001f);

  Fixture topEdgeFixture;
  topEdgeFixture.uuid = "top-edge-fixture";
  topEdgeFixture.transform = Translated(1510.0f, 160.0f, 300.0f);
  scene.fixtures[topEdgeFixture.uuid] = topEdgeFixture;
  auto topEdgeFixtureSnap = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::Fixture, topEdgeFixture.uuid});
  CHECK_OR_RETURN(topEdgeFixtureSnap);
  CHECK_OR_RETURN(topEdgeFixtureSnap->kind ==
                  magnet_snap::SnapKind::FixtureToTruss);
  CHECK_OR_RETURN(std::fabs(topEdgeFixtureSnap->translationDeltaMm[1] + 10.0f) <
                  0.001f);
  CHECK_OR_RETURN(std::fabs(topEdgeFixtureSnap->translationDeltaMm[2]) <
                  0.001f);

  CHECK_OR_RETURN(magnet_snap::ApplyCommittedSnapGrouping(scene, *fixtureSnap));
  CHECK_OR_RETURN(scene.fixtures[fixture.uuid].parentGroupUuid == groupUuid);
  CHECK_OR_RETURN(scene.fixtures[fixture.uuid].layer ==
                  scene.groupObjects[groupUuid].layer);
  CHECK_OR_RETURN(scene.trusses["target"].layer ==
                  scene.groupObjects[groupUuid].layer);

  SceneObject object;
  object.uuid = "object";
  object.transform = Translated(0.0f, 650.0f, 0.0f);
  scene.sceneObjects[object.uuid] = object;
  auto objectSnap = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::SceneObject, object.uuid});
  CHECK_OR_RETURN(objectSnap);
  CHECK_OR_RETURN(objectSnap->kind ==
                  magnet_snap::SnapKind::SceneObjectToObject);

  magnet_snap::SnapResult groupedTrussMove;
  groupedTrussMove.snapped = true;
  groupedTrussMove.sourceType = magnet_snap::ObjectType::Truss;
  groupedTrussMove.sourceUuid = "target";
  groupedTrussMove.translationDeltaMm = {0.0f, 0.0f, 100.0f};
  const float fixtureZBeforeGroupMove =
      scene.fixtures[fixture.uuid].transform.o[2];
  const float trussZBeforeGroupMove = scene.trusses["target"].transform.o[2];
  CHECK_OR_RETURN(magnet_snap::ApplySnapTransform(scene, groupedTrussMove));
  CHECK_OR_RETURN(std::fabs(scene.fixtures[fixture.uuid].transform.o[2] -
                            fixtureZBeforeGroupMove - 100.0f) < 0.001f);
  CHECK_OR_RETURN(std::fabs(scene.trusses["target"].transform.o[2] -
                            trussZBeforeGroupMove - 100.0f) < 0.001f);

  CHECK_OR_RETURN(magnet_snap::DetachSnapSourceFromGroup(scene, *fixtureSnap));
  CHECK_OR_RETURN(scene.fixtures[fixture.uuid].parentGroupUuid.empty());
  CHECK_OR_RETURN(scene.trusses["target"].parentGroupUuid == groupUuid);

  SetStage("temporary cleanup");
  std::filesystem::remove_all(tempRoot, filesystemError);
  CHECK_OR_RETURN(!filesystemError);

  SetStage("normal exit");
  return 0;
}
