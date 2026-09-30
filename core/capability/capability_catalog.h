#pragma once

#include "command/command_contract.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace perastage::capability {

enum class OperationKind { Command, Query };
enum class ArgumentType {
  Boolean,
  Int64,
  Float64,
  String,
  Int64List,
  Float64List,
  StringList
};
enum class Effect { ReadOnly, Mutating, Destructive };
enum class ExposureState { Full, Partial };

struct ArgumentDescriptor {
  std::string_view id;
  ArgumentType type;
  bool required;
  std::string_view description;
};

struct FrontendExposure {
  std::string_view frontendId;
  ExposureState state;
};

struct Descriptor {
  std::string_view operationId;
  OperationKind kind;
  std::string_view summary;
  Effect effect;
  std::vector<ArgumentDescriptor> arguments;
  std::vector<FrontendExposure> frontends;
};

enum class RequestShapeIssueKind {
  UnknownOperation,
  UnknownArgument,
  DuplicateArgument,
  WrongType,
  MissingArgument
};
struct RequestShapeIssue {
  RequestShapeIssueKind kind;
  std::string argumentId;
};

// Returns the immutable catalog in ascending operation-ID order.
std::span<const Descriptor> Catalog();
// Finds a descriptor by its stable operation identifier.
const Descriptor *Find(std::string_view operationId);
// Checks only the generic shape of a Command request against its descriptor.
std::vector<RequestShapeIssue>
ValidateRequestShape(const command::Request &request);

const char *Token(OperationKind value);
const char *Token(ArgumentType value);
const char *Token(Effect value);
const char *Token(ExposureState value);

} // namespace perastage::capability
