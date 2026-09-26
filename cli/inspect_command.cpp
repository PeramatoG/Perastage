#include "inspect_command.h"

#include "inspection/gdtf_inspection.h"
#include "inspection/inspection_json_serializer.h"
#include "inspection/mvr_inspection.h"
#include "inspection/resource_inspection.h"
#include "inspection_outcome.h"
#include "inspection_text_formatter.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <variant>

namespace perastage::cli {
namespace {

enum class View { Summary, Inventory, Resources, Diagnostics, Xml };

struct Options {
  std::filesystem::path path;
  View view = View::Summary;
  bool json = false;
};

// Emits a stable inspect usage error.
int UsageError(std::string_view message, std::ostream &err) {
  err << "perastage-cli inspect: " << message << '\n';
  err << "Try 'perastage-cli inspect --help' for usage.\n";
  return 2;
}

// Parses one supported human view token.
std::optional<View> ParseView(std::string_view value) {
  if (value == "summary")
    return View::Summary;
  if (value == "inventory")
    return View::Inventory;
  if (value == "resources")
    return View::Resources;
  if (value == "diagnostics")
    return View::Diagnostics;
  if (value == "xml")
    return View::Xml;
  return std::nullopt;
}

// Parses inspect arguments without interpreting the input file.
std::optional<Options> ParseOptions(std::span<const std::string_view> args,
                                    std::ostream &err, int &exitCode) {
  Options options;
  bool hasPath = false;
  bool hasView = false;
  for (std::size_t index = 0; index < args.size(); ++index) {
    const std::string_view argument = args[index];
    if (argument == "--json") {
      if (!hasPath) {
        exitCode = UsageError("input file must precede options.", err);
        return std::nullopt;
      }
      if (options.json) {
        exitCode = UsageError("duplicate --json option.", err);
        return std::nullopt;
      }
      if (hasView) {
        exitCode = UsageError("--json and --view are mutually exclusive.", err);
        return std::nullopt;
      }
      options.json = true;
    } else if (argument == "--view") {
      if (!hasPath) {
        exitCode = UsageError("input file must precede options.", err);
        return std::nullopt;
      }
      if (hasView) {
        exitCode = UsageError("duplicate --view option.", err);
        return std::nullopt;
      }
      if (options.json) {
        exitCode = UsageError("--json and --view are mutually exclusive.", err);
        return std::nullopt;
      }
      if (++index >= args.size()) {
        exitCode = UsageError("missing value for --view.", err);
        return std::nullopt;
      }
      const auto view = ParseView(args[index]);
      if (!view) {
        exitCode = UsageError("unknown view: " + std::string(args[index]), err);
        return std::nullopt;
      }
      options.view = *view;
      hasView = true;
    } else if (argument.starts_with('-')) {
      exitCode = UsageError("unknown option: " + std::string(argument), err);
      return std::nullopt;
    } else if (hasPath) {
      exitCode = UsageError(
          "extra positional argument: " + std::string(argument), err);
      return std::nullopt;
    } else {
      const auto *begin = reinterpret_cast<const char8_t *>(argument.data());
      options.path =
          std::filesystem::path(std::u8string(begin, begin + argument.size()));
      hasPath = true;
    }
  }
  if (!hasPath) {
    exitCode = UsageError("exactly one input file is required.", err);
    return std::nullopt;
  }
  return options;
}

// Returns a case-insensitive ASCII filesystem extension.
std::string LowerExtension(const std::filesystem::path &path) {
  const std::u8string raw = path.extension().u8string();
  std::string extension(raw.begin(), raw.end());
  std::transform(
      extension.begin(), extension.end(), extension.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return extension;
}

// Builds resource descriptors through the Core resource API.
std::vector<inspection::ResourceDescriptor> DescribeResources(
    const std::filesystem::path &path,
    const std::optional<inspection::PackageInventory> &inventory) {
  return inventory ? inspection::DescribePackageResources(path, *inventory)
                   : std::vector<inspection::ResourceDescriptor>{};
}

// Emits the selected GDTF representation from one in-memory inspection.
int RenderGdtf(const Options &options,
               const inspection::GdtfInspectionResult &result,
               const std::vector<inspection::ResourceDescriptor> &resources,
               std::ostream &out) {
  const int code = ClassifyInspectionOutcome(
      result.inspection, result.validation, result.document.has_value());
  if (options.json)
    out << inspection::serialization::SerializeGdtfReportToJson(result,
                                                                resources)
        << '\n';
  else if (options.view == View::Summary)
    out << FormatGdtfSummary(result, resources);
  else if (options.view == View::Inventory && result.packageInventory)
    out << FormatInventory(*result.packageInventory);
  else if (options.view == View::Resources)
    out << FormatResources(resources);
  else if (options.view == View::Diagnostics)
    out << FormatDiagnostics(result.inspection, result.validation);
  else if (options.view == View::Xml && result.document)
    out << result.document->Archive().descriptionXml;
  return code;
}

// Emits the selected MVR representation from one in-memory inspection.
int RenderMvr(const Options &options,
              const inspection::MvrInspectionResult &result,
              const std::vector<inspection::ResourceDescriptor> &resources,
              std::ostream &out) {
  const int code = ClassifyInspectionOutcome(
      result.inspection, result.validation, result.snapshot.has_value());
  if (options.json)
    out << inspection::serialization::SerializeMvrReportToJson(result,
                                                               resources)
        << '\n';
  else if (options.view == View::Summary)
    out << FormatMvrSummary(result, resources);
  else if (options.view == View::Inventory && result.packageInventory)
    out << FormatInventory(*result.packageInventory);
  else if (options.view == View::Resources)
    out << FormatResources(resources);
  else if (options.view == View::Diagnostics)
    out << FormatDiagnostics(result.inspection, result.validation);
  else if (options.view == View::Xml && result.snapshot)
    out << result.snapshot->sceneDescriptionXml;
  return code;
}

} // namespace

// Writes the exact inspect-specific grammar and view list.
void WriteInspectHelp(std::ostream &out) {
  out << "Usage: perastage-cli inspect <file> [--view <view> | --json]\n\n"
         "Views:\n"
         "  summary      Show the format-specific summary (default).\n"
         "  inventory    Show ordered package entries.\n"
         "  resources    Show ordered resource descriptors.\n"
         "  diagnostics  Show inspection and validation findings.\n"
         "  xml          Emit the retained root XML exactly.\n\n"
         "Options:\n"
         "  --view <view>  Select one human-readable view.\n"
         "  --json         Emit one complete structured report.\n"
         "  -h, --help     Show this help and exit.\n";
}

// Runs exactly one semantic inspection through the format-specific Core API.
int RunInspect(std::span<const std::string_view> args, std::ostream &out,
               std::ostream &err) {
  if (args.size() == 1 && (args.front() == "-h" || args.front() == "--help")) {
    WriteInspectHelp(out);
    return 0;
  }
  int parseCode = 2;
  const auto options = ParseOptions(args, err, parseCode);
  if (!options)
    return parseCode;
  const std::string extension = LowerExtension(options->path);
  if (extension == ".gdtf") {
    const auto result = inspection::InspectGdtf(options->path);
    return RenderGdtf(*options, result,
                      DescribeResources(options->path, result.packageInventory),
                      out);
  }
  if (extension == ".mvr") {
    const auto result = inspection::InspectMvr(options->path);
    return RenderMvr(*options, result,
                     DescribeResources(options->path, result.packageInventory),
                     out);
  }
  err << "perastage-cli inspect: unsupported input type; expected .gdtf or "
         ".mvr.\n";
  return 4;
}

} // namespace perastage::cli
