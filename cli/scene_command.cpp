#include "scene_command.h"

#include "command/command_json_serializer.h"
#include "external_scene_workflow.h"

#include <ostream>
#include <string>

namespace perastage::cli {
namespace {

// Writes scene-command help using stable, non-localized technical text.
void WriteHelp(std::ostream &out) {
  out << "Usage: perastage-cli scene <input.mvr> --output <output.mvr> "
         "--command <text> [--command <text> ...] [--overwrite] [--json]\n\n"
         "The input is read-only. The output must be a different explicit "
         "path.\n";
}

} // namespace

// Runs the explicit file-backed headless scene mutation workflow.
int RunScene(std::span<const std::string_view> args, std::ostream &out,
             std::ostream &err) {
  if (args.size() == 1 && (args.front() == "-h" || args.front() == "--help")) {
    WriteHelp(out);
    return 0;
  }
  if (args.empty()) {
    err << "perastage-cli scene: input MVR is required.\n";
    return 2;
  }
  external_scene::Request request;
  request.inputPath = std::string(args.front());
  bool json = false;
  for (std::size_t index = 1; index < args.size(); ++index) {
    const std::string_view argument = args[index];
    if (argument == "--output" || argument == "--command") {
      if (++index >= args.size()) {
        err << "perastage-cli scene: " << argument << " requires a value.\n";
        return 2;
      }
      if (argument == "--output")
        request.outputPath = std::string(args[index]);
      else
        request.commands.emplace_back(args[index]);
    } else if (argument == "--overwrite") {
      request.overwrite = true;
    } else if (argument == "--json") {
      json = true;
    } else {
      err << "perastage-cli scene: unknown option: " << argument << '\n';
      return 2;
    }
  }
  if (request.outputPath.empty() || request.commands.empty()) {
    err << "perastage-cli scene: --output and at least one --command are "
           "required.\n";
    return 2;
  }

  external_scene::Result result = external_scene::Execute(request);
  if (!result.success) {
    for (const std::string &diagnostic : result.diagnostics)
      err << "perastage-cli scene: " << diagnostic << '\n';
    return 4;
  }
  if (json) {
    out << "{\"schema_version\":1,\"success\":true,\"scene_changed\":"
        << (result.sceneChanged ? "true" : "false")
        << ",\"output_published\":true,\"command_results\":[";
    for (std::size_t index = 0; index < result.commandResults.size(); ++index) {
      if (index)
        out << ',';
      out << command::serialization::SerializeResultToJson(
          result.commandResults[index]);
    }
    out << "]}\n";
  } else {
    out << "Published canonical MVR: " << request.outputPath.string() << '\n';
  }
  return 0;
}

} // namespace perastage::cli
