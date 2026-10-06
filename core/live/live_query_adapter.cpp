#include "live/live_query_adapter.h"

#include "json.hpp"
#include "query/scene_query.h"

#include <optional>

namespace perastage::live {
namespace {
using Json = nlohmann::ordered_json;

Json Reference(const scene_identity::ObjectReference &object) {
  return {{"kind", scene_identity::KindToken(object.kind)},
          {"uuid", object.uuid}};
}

Json References(const std::vector<scene_identity::ObjectReference> &objects) {
  Json result = Json::array();
  for (const auto &object : objects)
    result.push_back(Reference(object));
  return result;
}

Json Descriptor(const query::ObjectDescriptor &object) {
  Json result = {{"kind", scene_identity::KindToken(object.kind)},
                 {"uuid", object.uuid},
                 {"name", object.name},
                 {"layer", object.layerUuid},
                 {"parent_group", object.parentGroupUuid},
                 {"type_name", object.typeName},
                 {"resource", object.resource}};
  if (object.fixtureIdText)
    result["fixture_id_text"] = *object.fixtureIdText;
  if (object.fixtureId)
    result["fixture_id"] = *object.fixtureId;
  if (object.fixtureIdNumeric)
    result["fixture_id_numeric"] = *object.fixtureIdNumeric;
  if (object.unitNumber)
    result["unit_number"] = *object.unitNumber;
  if (object.customId)
    result["custom_id"] = *object.customId;
  if (object.customIdType)
    result["custom_id_type"] = *object.customIdType;
  if (object.rawAddress)
    result["patch_address"] = *object.rawAddress;
  if (object.gdtfMode)
    result["gdtf_mode"] = *object.gdtfMode;
  return result;
}

std::optional<scene_identity::ObjectKind> ObjectKind(const std::string &token) {
  for (const auto kind : {scene_identity::ObjectKind::Fixture,
                          scene_identity::ObjectKind::Truss,
                          scene_identity::ObjectKind::Support,
                          scene_identity::ObjectKind::SceneObject,
                          scene_identity::ObjectKind::Group})
    if (token == scene_identity::KindToken(kind))
      return kind;
  return std::nullopt;
}

ExecutionResult InvalidArguments(const local_ipc::Request &request) {
  return {local_ipc::ErrorResponse(
              request.requestId, "invalid_arguments",
              "scene.object.get requires kind and nonempty uuid; other queries "
              "do not accept arguments."),
          {}};
}

} // namespace

ExecutionResult ExecuteQuery(const local_ipc::Request &request,
                             const command::ExecutionContext &context) {
  const Json arguments = request.argumentsJson.empty()
                             ? Json::object()
                             : Json::parse(request.argumentsJson);
  const auto &queryId = request.value;
  Json result = {{"query_id", queryId}};
  if (queryId == query::kObjectQueryId) {
    if (arguments.size() != 2 || !arguments.contains("kind") ||
        !arguments["kind"].is_string() || !arguments.contains("uuid") ||
        !arguments["uuid"].is_string() ||
        arguments["uuid"].get<std::string>().empty())
      return InvalidArguments(request);
    const auto kind = ObjectKind(arguments["kind"].get<std::string>());
    if (!kind)
      return InvalidArguments(request);
    std::vector<query::Diagnostic> diagnostics;
    const auto object = query::GetObject(
        context.scene, {*kind, arguments["uuid"].get<std::string>()}, diagnostics);
    if (!object)
      return {local_ipc::ErrorResponse(request.requestId, diagnostics.front().code,
                                      diagnostics.front().message),
              {}};
    result["object"] = Descriptor(*object);
    result["diagnostics"] = Json::array();
  } else {
    if (!arguments.empty())
      return InvalidArguments(request);
    if (queryId == query::kSummaryQueryId) {
      const auto summary = query::GetSummary(context.scene);
      result["mvr_version_major"] = summary.mvrVersionMajor;
      result["mvr_version_minor"] = summary.mvrVersionMinor;
      result["fixtures"] = summary.fixtures;
      result["trusses"] = summary.trusses;
      result["supports"] = summary.supports;
      result["scene_objects"] = summary.sceneObjects;
      result["groups"] = summary.groups;
      result["layers"] = summary.layers;
    } else if (queryId == query::kSelectionQueryId) {
      result["objects"] = References(query::GetSelection(context.selection));
    } else if (queryId == query::kObjectsQueryId) {
      result["objects"] = Json::array();
      for (const auto &object : query::ListObjects(context.scene))
        result["objects"].push_back(Descriptor(object));
    } else if (queryId == query::kLayersQueryId) {
      result["layers"] = Json::array();
      for (const auto &layer : query::ListLayers(context.scene))
        result["layers"].push_back({{"uuid", layer.uuid},
                                    {"name", layer.name},
                                    {"color", layer.color},
                                    {"members", References(layer.members)}});
    } else if (queryId == query::kGroupsQueryId) {
      result["groups"] = Json::array();
      for (const auto &group : query::ListGroups(context.scene))
        result["groups"].push_back(
            {{"uuid", group.uuid}, {"name", group.name},
             {"layer", group.layerUuid},
             {"parent_group", group.parentGroupUuid},
             {"children", References(group.children)}});
    } else {
      return {local_ipc::ErrorResponse(
                  request.requestId, "unsupported_operation",
                  "The requested query is not exposed by local live IPC."),
              {}};
    }
  }
  return {local_ipc::SuccessResponse(request.requestId, result.dump()), {}};
}

} // namespace perastage::live
