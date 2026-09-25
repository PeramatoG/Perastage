#include "inspection/xml_schema_validation.h"

#include "standard_schemas.h"

#include <libxml/parser.h>
#include <libxml/xmlschemas.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <utility>

namespace perastage::inspection {
namespace {

using XmlDoc = std::unique_ptr<xmlDoc, decltype(&xmlFreeDoc)>;
using SchemaParser =
    std::unique_ptr<xmlSchemaParserCtxt, decltype(&xmlSchemaFreeParserCtxt)>;
using Schema = std::unique_ptr<xmlSchema, decltype(&xmlSchemaFree)>;
using SchemaValidator =
    std::unique_ptr<xmlSchemaValidCtxt, decltype(&xmlSchemaFreeValidCtxt)>;

struct ErrorSink {
  std::vector<Diagnostic> *diagnostics = nullptr;
  DiagnosticLocation location;
  std::string code;
};

// Removes unstable whitespace emitted at the end of libxml2 messages.
std::string NormalizeMessage(std::string message) {
  while (!message.empty() && (message.back() == '\n' ||
                              message.back() == '\r' || message.back() == ' '))
    message.pop_back();
  return message;
}

// Collects a structured libxml2 error without exposing backend types publicly.
void CollectStructuredError(void *context, xmlError *error) {
  auto *sink = static_cast<ErrorSink *>(context);
  if (!sink || !sink->diagnostics || !error)
    return;
  DiagnosticLocation location = sink->location;
  if (error->line > 0)
    location.line = static_cast<std::uint32_t>(error->line);
  if (error->int2 > 0)
    location.column = static_cast<std::uint32_t>(error->int2);
  sink->diagnostics->push_back(
      {DiagnosticSeverity::Error, DiagnosticDomain::Xml,
       DiagnosticClassification::Standards, sink->code,
       NormalizeMessage(error->message ? error->message
                                       : "XML validation failed."),
       std::move(location)});
}

// Reads an XSD as immutable bytes from the repository schema set.
std::string ReadSchema(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}

// Constructs the stable result shell for the two independent validation stages.
XmlSchemaValidationResult NewResult(const SchemaDescriptor &descriptor) {
  XmlSchemaValidationResult result;
  result.xml.layer = ValidationLayer::XmlWellFormedness;
  result.schema.layer = ValidationLayer::Schema;
  result.schema.schema = descriptor.identity;
  return result;
}

} // namespace

// Validates immutable XML bytes locally, stopping schema work after malformed
// XML.
XmlSchemaValidationResult
ValidateXmlAgainstSchema(std::string_view xml,
                         const SchemaDescriptor &descriptor,
                         const DiagnosticLocation &sourceLocation) {
  static std::once_flag initialized;
  static std::mutex parserErrorMutex;
  std::call_once(initialized, [] { xmlInitParser(); });
  XmlSchemaValidationResult result = NewResult(descriptor);
  ErrorSink xmlErrors{&result.xml.diagnostics, sourceLocation,
                      "xml.well_formedness.invalid"};
  xmlParserCtxtPtr rawParser = xmlNewParserCtxt();
  if (!rawParser) {
    result.xml.status = ValidationStatus::Unavailable;
    return result;
  }
  std::unique_ptr<xmlParserCtxt, decltype(&xmlFreeParserCtxt)> parser(
      rawParser, &xmlFreeParserCtxt);
  XmlDoc document(nullptr, &xmlFreeDoc);
  {
    const std::lock_guard lock(parserErrorMutex);
    xmlSetStructuredErrorFunc(&xmlErrors, CollectStructuredError);
    document.reset(xmlCtxtReadMemory(
        parser.get(), xml.data(), static_cast<int>(xml.size()), "inspected.xml",
        nullptr, XML_PARSE_NONET));
    xmlSetStructuredErrorFunc(nullptr, nullptr);
  }
  if (!document) {
    result.xml.status = ValidationStatus::Invalid;
    result.schema.status = ValidationStatus::NotRun;
    return result;
  }
  result.xml.status = ValidationStatus::Valid;

  const std::string xsd =
      descriptor.xsd.empty() ? ReadSchema(descriptor.path) : descriptor.xsd;
  if (xsd.empty()) {
    result.schema.status = ValidationStatus::Unavailable;
    result.schema.diagnostics.push_back(
        {DiagnosticSeverity::Error, DiagnosticDomain::Content,
         DiagnosticClassification::Standards, "schema.unavailable",
         "The repository-controlled validation schema is unavailable.",
         sourceLocation});
    return result;
  }
  if (xsd.find("<xs:import") != std::string::npos ||
      xsd.find("<xs:include") != std::string::npos) {
    result.schema.status = ValidationStatus::Unavailable;
    result.schema.diagnostics.push_back(
        {DiagnosticSeverity::Error, DiagnosticDomain::Content,
         DiagnosticClassification::Standards,
         "schema.external_dependency_prohibited",
         "The validation schema requests an unsupported external dependency.",
         sourceLocation});
    return result;
  }

  SchemaParser schemaParser(
      xmlSchemaNewMemParserCtxt(xsd.data(), static_cast<int>(xsd.size())),
      &xmlSchemaFreeParserCtxt);
  ErrorSink schemaErrors{&result.schema.diagnostics, sourceLocation,
                         "schema.definition.invalid"};
  if (schemaParser)
    xmlSchemaSetParserStructuredErrors(schemaParser.get(),
                                       CollectStructuredError, &schemaErrors);
  Schema compiled(schemaParser ? xmlSchemaParse(schemaParser.get()) : nullptr,
                  &xmlSchemaFree);
  if (!compiled) {
    result.schema.status = ValidationStatus::Unavailable;
    return result;
  }
  SchemaValidator validator(xmlSchemaNewValidCtxt(compiled.get()),
                            &xmlSchemaFreeValidCtxt);
  schemaErrors.code = "schema.document.invalid";
  if (validator)
    xmlSchemaSetValidStructuredErrors(validator.get(), CollectStructuredError,
                                      &schemaErrors);
  const int status =
      validator ? xmlSchemaValidateDoc(validator.get(), document.get()) : -1;
  result.schema.status = status == 0
                             ? ValidationStatus::Valid
                             : (status > 0 ? ValidationStatus::Invalid
                                           : ValidationStatus::Unavailable);
  return result;
}

// Returns the pinned Perastage GDTF 1.2 schema descriptor.
SchemaDescriptor Gdtf12Schema() {
  return {{"gdtf", "1.2", "perastage-1", "perastage:gdtf:1.2",
          "098d3791f77f0895bd859adf01864b4826e2006f"},
          std::filesystem::path(PERASTAGE_STANDARD_SCHEMA_DIR) / "gdtf" /
              "1.2" / "gdtf-perastage.xsd",
          std::string(schemas::kGdtf12)};
}

// Returns the pinned Perastage MVR 1.6 schema descriptor.
SchemaDescriptor Mvr16Schema() {
  return {{"mvr", "1.6", "perastage-1", "perastage:mvr:1.6",
          "098d3791f77f0895bd859adf01864b4826e2006f"},
          std::filesystem::path(PERASTAGE_STANDARD_SCHEMA_DIR) / "mvr" / "1.6" /
              "mvr-perastage.xsd",
          std::string(schemas::kMvr16)};
}

} // namespace perastage::inspection
