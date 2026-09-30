#include "command_json_serializer.h"

#include "json.hpp"

namespace perastage::command::serialization {
namespace {

// Serializes one typed argument value without coercing its numeric type.
nlohmann::json SerializeValue(const ArgumentValue &value) {
  return std::visit(
      [](const auto &typedValue) { return nlohmann::json(typedValue); }, value);
}

// Serializes a semantic request while retaining argument order.
nlohmann::json SerializeRequest(const Request &request) {
  nlohmann::json arguments = nlohmann::json::array();
  for (const Argument &argument : request.arguments) {
    arguments.push_back(
        {{"id", argument.id}, {"value", SerializeValue(argument.value)}});
  }
  return {{"command_id", request.commandId},
          {"arguments", std::move(arguments)}};
}

// Serializes one structured diagnostic using stable machine tokens.
nlohmann::json SerializeDiagnostic(const Diagnostic &diagnostic) {
  nlohmann::json serialized{{"severity", SeverityToken(diagnostic.severity)},
                            {"phase", PhaseToken(diagnostic.phase)},
                            {"code", diagnostic.code},
                            {"message", diagnostic.message}};
  serialized["argument_id"] = diagnostic.argumentId
                                  ? nlohmann::json(*diagnostic.argumentId)
                                  : nlohmann::json(nullptr);
  return serialized;
}

// Serializes semantic mutation facts without frontend refresh instructions.
nlohmann::json SerializeMutation(const MutationSummary &mutation) {
  return {{"scene_changed", mutation.sceneChanged},
          {"selection_changed", mutation.selectionChanged},
          {"project_metadata_changed", mutation.projectMetadataChanged},
          {"undo_entry_recorded", mutation.undoEntryRecorded},
          {"project_dirty", mutation.projectDirty}};
}

} // namespace

// Serializes a command result to deterministic compact schema-version-1 JSON.
std::string SerializeResultToJson(const Result &result) {
  nlohmann::json diagnostics = nlohmann::json::array();
  for (const Diagnostic &diagnostic : result.diagnostics)
    diagnostics.push_back(SerializeDiagnostic(diagnostic));

  nlohmann::json outputs = nlohmann::json::array();
  for (const Argument &output : result.outputs) {
    outputs.push_back(
        {{"id", output.id}, {"value", SerializeValue(output.value)}});
  }
  nlohmann::json serialized{{"schema_version", kCommandJsonSchemaVersion},
                            {"request", nullptr},
                            {"outcome", OutcomeToken(result.outcome)},
                            {"diagnostics", std::move(diagnostics)},
                            {"mutation", SerializeMutation(result.mutation)},
                            {"outputs", std::move(outputs)}};
  if (result.request)
    serialized["request"] = SerializeRequest(*result.request);
  return serialized.dump();
}

} // namespace perastage::command::serialization
