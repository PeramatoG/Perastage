#include "command/command_json_serializer.h"

#include "json.hpp"

#include <cassert>
#include <cstdint>

using namespace perastage::command;

// Verifies deterministic schema-version-1 command result serialization.
int main() {
  Request request{"test.command",
                  {{"enabled", true},
                   {"count", std::int64_t{7}},
                   {"distance", 2.5},
                   {"name", std::string("fixture")},
                   {"ids", std::vector<std::int64_t>{1, 2}},
                   {"values", std::vector<double>{1.25, 2.5}},
                   {"names", std::vector<std::string>{"a", "b"}}}};
  Result result{request, Outcome::Success, {}, {}};
  result.diagnostics.push_back(
      {DiagnosticSeverity::Warning, DiagnosticPhase::Validation,
       "command.test.warning", "Test warning.", "distance"});
  result.mutation = {true, true, false, true, true};
  result.outputs.push_back(
      {"affected_uuids", std::vector<std::string>{"a", "b"}});

  const std::string first = serialization::SerializeResultToJson(result);
  const std::string second = serialization::SerializeResultToJson(result);
  assert(first == second);

  const nlohmann::json parsed = nlohmann::json::parse(first);
  assert(parsed["schema_version"] == 1);
  assert(parsed["outcome"] == "success");
  assert(parsed["diagnostics"][0]["severity"] == "warning");
  assert(parsed["diagnostics"][0]["phase"] == "validation");
  assert(parsed["request"]["arguments"][0]["value"].is_boolean());
  assert(parsed["request"]["arguments"][1]["value"].is_number_integer());
  assert(parsed["request"]["arguments"][2]["value"].is_number_float());
  assert(parsed["request"]["arguments"][4]["value"][1] == 2);
  assert(parsed["request"]["arguments"][5]["value"][0] == 1.25);
  assert(parsed["request"]["arguments"][6]["value"][1] == "b");
  assert(parsed["mutation"]["scene_changed"] == true);
  assert(parsed["mutation"]["undo_entry_recorded"] == true);
  assert(parsed["outputs"][0]["id"] == "affected_uuids");
  assert(parsed["outputs"][0]["value"][1] == "b");

  Result parseFailure{std::nullopt, Outcome::ParseError, {}, {}};
  parseFailure.diagnostics.push_back(
      {DiagnosticSeverity::Error, DiagnosticPhase::Parse,
       "command.parse.invalid", "Invalid input.", std::nullopt});
  const nlohmann::json failed =
      nlohmann::json::parse(serialization::SerializeResultToJson(parseFailure));
  assert(failed["request"].is_null());
  assert(failed["outcome"] == "parse_error");
  assert(failed["diagnostics"][0]["severity"] == "error");
  assert(failed["diagnostics"][0]["phase"] == "parse");
  assert(failed["mutation"]["project_dirty"] == false);
  return 0;
}
