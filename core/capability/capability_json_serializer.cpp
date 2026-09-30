#include "capability/capability_json_serializer.h"

#include "capability/capability_catalog.h"
#include "json.hpp"

namespace perastage::capability {

// Serializes the complete capability catalog using discovery schema version 1.
std::string SerializeCatalogJson() {
  nlohmann::ordered_json operations = nlohmann::ordered_json::array();
  for (const Descriptor &descriptor : Catalog()) {
    nlohmann::ordered_json arguments = nlohmann::ordered_json::array();
    for (const ArgumentDescriptor &argument : descriptor.arguments)
      arguments.push_back({{"id", argument.id},
                           {"type", Token(argument.type)},
                           {"required", argument.required},
                           {"description", argument.description}});
    nlohmann::ordered_json frontends = nlohmann::ordered_json::array();
    for (const FrontendExposure &frontend : descriptor.frontends)
      frontends.push_back(
          {{"id", frontend.frontendId}, {"state", Token(frontend.state)}});
    operations.push_back({{"operation_id", descriptor.operationId},
                          {"kind", Token(descriptor.kind)},
                          {"effect", Token(descriptor.effect)},
                          {"summary", descriptor.summary},
                          {"arguments", arguments},
                          {"frontend_exposure", frontends}});
  }
  return nlohmann::ordered_json{{"schema_version", 1},
                                {"operations", operations}}
             .dump(2) +
         '\n';
}

} // namespace perastage::capability
